//
// Created by Romain on 26/02/2023.
//

#include <nvs_flash.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "ouchat_wifi.h"
#include "ouchat_api.h"
#include "ouchat_led.h"
#include "esp_log.h"
#include "ouchat_logger.h"
#include "esp_netif.h"

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;

static void wevent_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {

        esp_wifi_connect();

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        //Connecting to the AP again
        esp_wifi_connect();
    }
}

void ouchat_wait_wifi(){
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);
}

void ouchat_wifi_register_events(){
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wevent_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wevent_handler, NULL);
}

void ouchat_wifi_wakeup(void *value){

    //Init nvs flash
    esp_err_t nvs_ret = nvs_flash_init();

    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    //Start the Wi-Fi
    ouchat_wifi_register_events();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    ouchat_wait_wifi();

    esp_netif_init();

    if (ouchat_api_status == WAITING_WIFI){
        ouchat_api_status = 0;
    }

    if (value != NULL){
        return;
    }

    vTaskDelete(NULL);
}