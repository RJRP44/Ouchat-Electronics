//
// Created by Romain on 26/02/2023.
//

#include <stdio.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include <stdbool.h>
#include "ouchat_wifi_prov.h"
#include "ouchat_prov_srp.h"
#include "sdkconfig.h"

//get the device name in the form : Ouchat-CATID
void get_device_name(char *service_name, size_t max){
    snprintf(service_name, max, "Ouchat-%s", CONFIG_OUCHAT_CAT);
}

void ouchat_get_sec2_salt(const char **salt, uint16_t *salt_len) {
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
}

void ouchat_get_sec2_verifier(const char **verifier, uint16_t *verifier_len) {
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
}

void ouchat_init_provisioning(){
    //Wi-Fi Provisioning configuration
    wifi_prov_mgr_config_t config = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };

    wifi_prov_mgr_init(config);
}

bool ouchat_is_provisioned(){
    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);
    return provisioned;
}

void ouchat_start_provisioning(){

    //Define the device name for the ble
    char ouchat_device_name[13];
    get_device_name(ouchat_device_name,sizeof ouchat_device_name);

    wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
    wifi_prov_security2_params_t sec2_params = {};

    ouchat_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len);
    ouchat_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len);

    wifi_prov_security2_params_t *sec_params = &sec2_params;

    uint8_t service_uuid[] = {
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };

    wifi_prov_scheme_ble_set_service_uuid(service_uuid);

    wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, ouchat_device_name, NULL);
}

void ouchat_deinit_provisioning(){
    wifi_prov_mgr_deinit();
}