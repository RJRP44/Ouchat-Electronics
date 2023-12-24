//
// Created by RJRP on 19/06/2023.
//

#include <lwip/sockets.h>
#include <esp_sleep.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "ouchat_logger.h"
#include "ouchat_wifi.h"
#include "cJSON.h"
#include "vl53l8cx_api.h"
#include "ouchat_sensor.h"

static const char *TAG = "tcp_client";

#if CONFIG_OUCHAT_DEBUG_LOGGER
char host_ip[] = CONFIG_OUCHAT_DEBUG_LOGGER_IP;
#else
char host_ip[] = "0.0.0.0";
#endif

static QueueHandle_t log_queue;
static QueueHandle_t deep_sleep_queue;
static EventGroupHandle_t logger_event_group;

uint8_t ouchat_logger_init() {
    log_queue = xQueueCreate(OUCHAT_LOG_QUEUE_SIZE, OUCHAT_LOG_SIZE);
    deep_sleep_queue = xQueueCreate(1, 1);
    logger_event_group = xEventGroupCreate();
    return 0;
}

uint8_t ouchat_log(unsigned char *data, size_t size) {

    //12 for timestamp + 172 for base64 data + \0
    size_t buffer_size = 12 + 172 + 1;
    char buffer [buffer_size];
    uint32_t time = esp_log_timestamp();

    //Add timestamp to the data
    snprintf(buffer, buffer_size, "(%lu)%s", time, data);

    xQueueSend(log_queue, buffer, 0);
    return 0;
}

uint8_t ouchat_stop_logger() {
    xQueueSend(deep_sleep_queue, "1", 0);
    xEventGroupWaitBits(logger_event_group, OUCHAT_LOG_EVENT_STOPPED, false, true, portMAX_DELAY);
    return 0;
}

void ouchat_logger(void *arg) {
    char log[190];

    struct sockaddr_in serv_addr;
    inet_pton(AF_INET, host_ip, &serv_addr.sin_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(3010);

    int s;

    uint8_t option = 2;
    ouchat_wifi_wakeup(&option);

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket < 0) {
        ESP_LOGE(TAG, "... Failed to allocate socket.\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    if (connect(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        ESP_LOGE(TAG, "... socket connect failed errno=%d \n", errno);
        close(s);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        return;
    }

    cJSON *packet = cJSON_CreateObject();

    cJSON_AddStringToObject(packet, "cat", CONFIG_OUCHAT_CAT);
    cJSON_AddNumberToObject(packet, "context", *((uint16_t *) arg));

    uint16_t packet_len = strlen(cJSON_PrintUnformatted(packet));
    char *text_packet;
    text_packet = malloc(packet_len + 1);
    memcpy(text_packet, cJSON_PrintUnformatted(packet), packet_len);
    *(text_packet + packet_len) = '\n';
    *(text_packet + packet_len + 1) = '\0';
    write(s, text_packet, strlen(text_packet));

    free(text_packet);
    cJSON_Delete(packet);
    while (1) {
        unsigned int msg_queue_lenght = uxQueueMessagesWaiting(log_queue);
        if (msg_queue_lenght == 0 && uxQueueMessagesWaiting(deep_sleep_queue) > 0) {

            shutdown(s, SHUT_RDWR);

            //Wait before closing the socket
            vTaskDelay(500 / portTICK_PERIOD_MS);
            close(s);

            xEventGroupSetBits(logger_event_group, OUCHAT_LOG_EVENT_STOPPED);
        }
        if (msg_queue_lenght >= 1) {
            xQueueReceive(log_queue, log, 0);
            char *end = log + strlen(log);
            if (write(s, log, strlen(log)) < 0) {
                ESP_LOGE(TAG, "... Send failed \n");
                close(s);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }

        }

        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}