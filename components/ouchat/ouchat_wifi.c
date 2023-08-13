//
// Created by Romain on 26/02/2023.
//

#include <wifi_provisioning/manager.h>
#include <nvs_flash.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "ouchat_wifi_prov.h"
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
    static int retries;

    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                //Provisioning started
                break;
            case WIFI_PROV_CRED_RECV:
                //Received Wi-Fi credentials
                break;
            case WIFI_PROV_CRED_FAIL: {
                //Provisioning failed!
                wifi_prov_mgr_reset_sm_state_on_failure();

                break;
            }
            case WIFI_PROV_CRED_SUCCESS:{
                //Provisioning successful

                TaskHandle_t xHandle = NULL;
                xTaskCreate(ouchat_api_join, "ouchat_api_join", 8192, (void *) ouchat_provisioner_token, 5, &xHandle);
                configASSERT(xHandle);


                break;
            }
            case WIFI_PROV_END:
                // De-initialize manager once provisioning is finished
                ouchat_deinit_provisioning();

                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {

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

    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wevent_handler, NULL);
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

    ouchat_api_status = 0;

    vTaskDelete(NULL);
}