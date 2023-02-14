//
// Created by Romain on 14/02/2023.
//

#include "stdlib.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "sdkconfig.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "ouchat_ble.h"
#include "ouchat_ble_events.h"
#include "nvs_flash.h"

esp_err_t ouchat_ble_init() {
    esp_err_t ret;

    /* Initialize NVS. */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());
    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "gatts register error, error code = %x", ret);
        return ret;
    }
    ESP_LOGI(OUCHAT_BLE_TAG, "free heap : %lu", xPortGetFreeHeapSize());
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "gap register error, error code = %x", ret);
        return ret;
    }

    ret = esp_ble_gatts_app_register(OUCHAT_APP_ID);
    if (ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "gatts app register error, error code = %x", ret);
        return ret;
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(OUCHAT_BLE_TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }
    return 0;
}