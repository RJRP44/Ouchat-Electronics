//
// Created by Romain on 14/02/2023.
//

#include <esp_log.h>
#include <memory.h>
#include "ouchat_ble_events.h"
#include "ouchat_ble.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"

static const uint16_t SVC_UUID_OCA = 0x00FF;
static const uint16_t CHAR_UUID_OCA_CAT = 0xFF01;

uint16_t heart_rate_handle_table[HRS_OCA_NB];

static uint8_t adv_config_done = 0;
static uint8_t raw_adv_data[] = {
        /* flags */
        0x02, 0x01, 0x06,
        /* tx power*/
        0x02, 0x0a, 0xeb,
        /* service uuid */
        0x03, 0x03, 0xFF, 0x00,
        //Device name
        0x75,0x63,0x68,0x61,0x74};

static uint8_t raw_scan_rsp_data[] = {
        /* flags */
        0x02, 0x01, 0x06,
        /* tx power */
        0x02, 0x0a, 0xeb,
        /* service uuid */
        0x03, 0x03, 0xFF,0x00
};

static esp_ble_adv_params_t adv_params = {
        .adv_int_min         = 0x20,
        .adv_int_max         = 0x40,
        .adv_type            = ADV_TYPE_IND,
        .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
        .channel_map         = ADV_CHNL_ALL,
        .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static const uint16_t svc_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t char_uuid = ESP_GATT_UUID_CHAR_DECLARE;

static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
//static const uint8_t char_prop_write = ESP_GATT_CHAR_PROP_BIT_WRITE;
//static const uint8_t char_prop_read_write_notify = ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static const uint8_t char_cat_value[] = CONFIG_OUCHAT_CAT;

static const esp_gatts_attr_db_t gatt_db[HRS_OCA_NB] =
        {
                //Ouchat Service Declaration
                [OCA_SVC] = {{ESP_GATT_AUTO_RSP},
                             {ESP_UUID_LEN_16, (uint8_t *) &svc_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(SVC_UUID_OCA), (uint8_t *) &SVC_UUID_OCA}},

                //Characteristic Cat Declaration
                [OCA_CHAR_CAT] = {{ESP_GATT_AUTO_RSP},
                                  {ESP_UUID_LEN_16, (uint8_t *) &char_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *) &char_prop_read}},

                //Characteristic Cat Value Declaration
                [OCA_CHAR_VAL_CAT] = {{ESP_GATT_AUTO_RSP},
                                      {ESP_UUID_LEN_16, (uint8_t *)&CHAR_UUID_OCA_CAT, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(char_cat_value), sizeof(char_cat_value), (uint8_t *)char_cat_value}}
        };


static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(DEVICE_NAME);
            if (set_dev_name_ret) {
                ESP_LOGE(OUCHAT_BLE_TAG, "set device name failed, error code = %x", set_dev_name_ret);
            }

            esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(raw_adv_data, sizeof(raw_adv_data));
            if (raw_adv_ret) {
                ESP_LOGE(OUCHAT_BLE_TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
            }
            adv_config_done |= ADV_CONFIG_FLAG;
            esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, sizeof(raw_scan_rsp_data));
            if (raw_scan_ret) {
                ESP_LOGE(OUCHAT_BLE_TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
            }
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HRS_OCA_NB, SVC_INST_ID);
            if (create_attr_ret) {
                ESP_LOGE(OUCHAT_BLE_TAG, "create attr table failed, error code = %x", create_attr_ret);
            }
        }
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(OUCHAT_BLE_TAG, "ESP_GATTS_CONNECT_EVT, conn_id = %d", param->connect.conn_id);
                    esp_log_buffer_hex(OUCHAT_BLE_TAG, param->connect.remote_bda, 6);
            esp_ble_conn_update_params_t conn_params = {0};
            memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            /* For the iOS system, please refer to Apple official documents about the BLE connection parameters restrictions. */
            conn_params.latency = 0;
            conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
            conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
            conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
            //start sent the update connection parameters to the peer device.
            esp_ble_gap_update_conn_params(&conn_params);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(OUCHAT_BLE_TAG, "ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", param->disconnect.reason);
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:{
            if (param->add_attr_tab.status != ESP_GATT_OK){
                ESP_LOGE(OUCHAT_BLE_TAG, "create attribute table failed, error code=0x%x", param->add_attr_tab.status);
            }
            else if (param->add_attr_tab.num_handle != HRS_OCA_NB){
                ESP_LOGE(OUCHAT_BLE_TAG, "create attribute table abnormally, num_handle (%d) \
                        doesn't equal to HRS_IDX_NB(%d)", param->add_attr_tab.num_handle, HRS_OCA_NB);
            }
            else {
                ESP_LOGI(OUCHAT_BLE_TAG, "create attribute table successfully, the number handle = %d\n",param->add_attr_tab.num_handle);
                memcpy(heart_rate_handle_table, param->add_attr_tab.handles, sizeof(heart_rate_handle_table));
                esp_ble_gatts_start_service(heart_rate_handle_table[OCA_SVC]);
            }
            break;
        }
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_UNREG_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            break;
    }
}


static struct gatts_profile_inst heart_rate_profile_tab[PROFILE_NUM] = {
        [PROFILE_APP_OCA] = {
                .gatts_cb = gatts_profile_event_handler,
                .gatts_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        },
};

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param){    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            heart_rate_profile_tab[PROFILE_APP_OCA].gatts_if = gatts_if;
        } else {
            ESP_LOGE(OUCHAT_BLE_TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }
    do {
        int oca;
        for (oca = 0; oca < PROFILE_NUM; oca++) {
            /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
            if (gatts_if == ESP_GATT_IF_NONE || gatts_if == heart_rate_profile_tab[oca].gatts_if) {
                if (heart_rate_profile_tab[oca].gatts_cb) {
                    heart_rate_profile_tab[oca].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
    switch (event) {

        case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0){
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            /* advertising start complete event to indicate advertising start successfully or failed */
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(OUCHAT_BLE_TAG, "advertising start failed");
            }else{
                ESP_LOGI(OUCHAT_BLE_TAG, "advertising start successfully");
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(OUCHAT_BLE_TAG, "Advertising stop failed");
            }
            else {
                ESP_LOGI(OUCHAT_BLE_TAG, "Stop adv successfully\n");
            }
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(OUCHAT_BLE_TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                     param->update_conn_params.status,
                     param->update_conn_params.min_int,
                     param->update_conn_params.max_int,
                     param->update_conn_params.conn_int,
                     param->update_conn_params.latency,
                     param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}