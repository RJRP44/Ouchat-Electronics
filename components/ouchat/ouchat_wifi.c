//
// Created by Romain on 26/02/2023.
//

#include <wifi_provisioning/manager.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "ouchat_wifi_prov.h"
#include "ouchat_wifi.h"

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

                retries++;
                if (retries >= 5) {
                    //Failed to connect with provisioned AP, reseting provisioned credentials
                    wifi_prov_mgr_reset_sm_state_on_failure();
                    retries = 0;
                }

                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                //Provisioning successful

                retries = 0;

                break;
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

void ouchat_wifi_register_handlers(){
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, false, true, portMAX_DELAY);
}

void ouchat_wifi_register_events(){
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wevent_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wevent_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wevent_handler, NULL);

}