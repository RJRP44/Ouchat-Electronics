//
// Created by Romain on 28/01/2024.
//

#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <wifi.h>

static EventGroupHandle_t wifi_event_group;
static uint8_t retry = 0;

static void event_sta_start_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    esp_wifi_connect();
}

static void event_connected_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED);
}

static void event_disconnected_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {

    //Retry
    if (retry < MAXIMUM_RETRY) {
        esp_wifi_connect();
        retry++;
    } else {
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL);
        retry = 0;
    }
}

esp_err_t register_events_handlers() {

    esp_err_t status = ESP_OK;

    status |= esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, event_disconnected_handler,NULL, NULL);
    status |= esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, event_sta_start_handler, NULL,NULL);
    status |= esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_connected_handler, NULL, NULL);

    return status;
}

void wifi_init(void *value) {

    //Init event group
    wifi_event_group = xEventGroupCreate();

    //Init NVS Flash
    esp_err_t status = nvs_flash_init();
    if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        status = nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&config);

    register_events_handlers();

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    //Wait for the results
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,WIFI_CONNECTED | WIFI_FAIL,pdFALSE,pdFALSE,portMAX_DELAY);

    //If this function is used as a threaded task the value mustn't be null
    if (value == THREADED_TASK){

        //Delete the task
        vTaskDelete(NULL);
        return;
    }

    //Return the result
    *((bool *) value) = bits & WIFI_CONNECTED ? true : false;
}