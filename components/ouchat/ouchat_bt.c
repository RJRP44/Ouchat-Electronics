//
// Created by Romain on 23/10/2022.
//

#include "ouchat_bt.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "cJSON.h"
#include "string.h"

void ouchat_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param){

    //Secure Simple Pairing
    if(event == ESP_BT_GAP_CFM_REQ_EVT){
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %d", param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
    }
}

void ouchat_bt_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){

    //Serial Port Protocol events
    switch (event) {

        //Init the bluetooth serial device
        case ESP_SPP_INIT_EVT:
            esp_bt_dev_set_device_name(DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            esp_spp_start_srv(sec_mask,role_slave, 0, SPP_SERVER_NAME);
            break;

            //When data is received from the SPP connection
        case ESP_SPP_DATA_IND_EVT:

            esp_log_buffer_char("Received String Data",param->data_ind.data,param->data_ind.len);


            cJSON *aps = NULL;
            cJSON *ap = NULL;
            cJSON *ap_ssid = NULL;
            cJSON *ap_rssi = NULL;
            cJSON *ap_encryption = NULL;

            aps = cJSON_CreateArray();

            ap = cJSON_CreateObject();

            ap_rssi = cJSON_CreateNumber(10);
            cJSON_AddItemToObject(ap,"rssi",ap_rssi);

            cJSON_AddItemToArray(aps,ap);


            char* message = "Received";
            uint8_t* data = (uint8_t *) cJSON_PrintUnformatted(aps);
            esp_spp_write(param->srv_open.handle, strlen( cJSON_PrintUnformatted(aps)), data);

            cJSON_Delete(aps);
            break;

        default:
            break;
    }
}

void ouchat_bt_init(){
    ESP_LOGI(SPP_TAG, "Initializing bluetooth");

    //Initialize the default NVS partition.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    //Release the controller memory as per the mode
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    //Initialize the bluetooth controller
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Controller init ok");

    //Enable the bluetooth controller
    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Enable controller ok");

    //Initialize Bluedroid
    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Bluedroid init ok");

    //Enable Bluedroid
    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Enable bluedroid ok");

    //Register generic access profile Callback
    if ((ret = esp_bt_gap_register_callback(ouchat_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Gap register ok");

    //Register serial protocol port callback
    if ((ret = esp_spp_register_callback(ouchat_bt_spp_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Spp register ok");


    if ((ret = esp_spp_init(esp_spp_mode)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(SPP_TAG, "Spp init ok");

    //Enables the Secure Simple Pairing.

    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    ESP_LOGI(SPP_TAG, "Set parameters");
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 6, pin_code);
}