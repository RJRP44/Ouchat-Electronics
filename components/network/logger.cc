//
// Created by Romain on 28/01/2024.
//
#include <lwip/sockets.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <cJSON.h>
#include <mbedtls/base64.h>
#include <ouchat_types.h>
#include "logger.h"
#include "wifi.h"
#include <string>

#define LOG_TAG "ouchat-logger"

#if CONFIG_OUCHAT_DEBUG_LOGGER
char host_ip[] = CONFIG_OUCHAT_DEBUG_LOGGER_IP;
#else
char host_ip[] = "0.0.0.0";
#endif

static QueueHandle_t log_queue;
static QueueHandle_t deep_sleep_queue;
static EventGroupHandle_t logger_event_group;

uint8_t init_tcp_logger() {
    log_queue = xQueueCreate(OUCHAT_LOG_QUEUE_SIZE, OUCHAT_LOG_SIZE);
    deep_sleep_queue = xQueueCreate(1, 1);
    logger_event_group = xEventGroupCreate();
    return 0;
}

uint8_t tcp_log(unsigned char *data) {

    //12 for timestamp + 172 for base64 data + \0
    size_t buffer_size = 12 + 172 + 1;
    char buffer [buffer_size];
    uint32_t time = esp_log_timestamp();

    //Add timestamp to the data
    snprintf(buffer, buffer_size, "(%lu)%p", time, data);

    xQueueSend(log_queue, buffer, 0);
    return 0;
}

uint8_t stop_tcp_logger() {
    ESP_LOGI(LOG_TAG, "Closing the logging socket...");

    //Send the stop instruction
    xQueueSend(deep_sleep_queue, "1", 0);
    xEventGroupWaitBits(logger_event_group, OUCHAT_LOG_EVENT_STOPPED, false, true, portMAX_DELAY);
    return 0;
}

void tcp_logger_task(void *args) {
    char log[190];

    struct sockaddr_in serv_addr{};
    inet_pton(AF_INET, host_ip, &serv_addr.sin_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(3010);

    bool status = false;
    wifi_init(&status);

    //If not connected to the Wi-Fi cancel
    if (!status){
        ESP_LOGE(LOG_TAG, "Not connected to Wi-Fi, cancelling...");
        return;
    }

    int sock  = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(LOG_TAG, "Failed to allocate socket...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    if (connect(sock, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) != 0) {
        ESP_LOGE(LOG_TAG, "Socket connect failed errno=%d", errno);

        //Stop the socket
        close(sock);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        return;
    }

    //The context information in json
    cJSON *packet = cJSON_CreateObject();
    cJSON_AddStringToObject(packet, "cat", CONFIG_OUCHAT_CAT);

    size_t length;
    unsigned char output[961];
    mbedtls_base64_encode(output, 961, &length, static_cast<const unsigned char *>(args), sizeof(calibration_config_t));
    cJSON_AddStringToObject(packet, "context", reinterpret_cast<char *>(output));

    uint16_t packet_len = strlen(cJSON_PrintUnformatted(packet));
    std::string text_packet;
    text_packet.resize(packet_len);

    memcpy(text_packet.data(), cJSON_PrintUnformatted(packet), packet_len);

    //Define the end of the string
    text_packet.append("\n");

    //Send the packet
    write(sock, text_packet.c_str(), text_packet.size());
    cJSON_Delete(packet);

    while (true) {
        unsigned int queue_length = uxQueueMessagesWaiting(log_queue);
        if (queue_length == 0 && uxQueueMessagesWaiting(deep_sleep_queue) > 0) {

            shutdown(sock, SHUT_RDWR);

            //Wait before closing the socket
            vTaskDelay(500 / portTICK_PERIOD_MS);
            close(sock);

            xEventGroupSetBits(logger_event_group, OUCHAT_LOG_EVENT_STOPPED);
            vTaskDelete(nullptr);
            return;
        }
        if (queue_length >= 1) {
            xQueueReceive(log_queue, log, 0);
            if (write(sock, log, strlen(log)) < 0) {
                ESP_LOGE(LOG_TAG, "Send failed...");

                //Stop the socket
                close(sock);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }
        }

        vTaskDelay(15 / portTICK_PERIOD_MS);
    }
}