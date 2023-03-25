//
// Created by RJRP on 25/03/2023.
//

#include <string.h>
#include <protocomm.h>
#include <protocomm_ble.h>
#include <protocomm_security2.h>
#include "ouchat_protocomm.h"
#include "ouchat_wifi_prov.h"
#include "esp_log.h"

esp_err_t echo_req_handler(uint32_t session_id,
                           const uint8_t *inbuf, ssize_t inlen,
                           uint8_t **outbuf, ssize_t *outlen,
                           void *priv_data) {
    /* Session ID may be used for persistence */
    printf("Session ID : %lu", session_id);

    /* Echo back the received data */
    *outlen = inlen;            /* Output data length updated */
    *outbuf = malloc(inlen);    /* This will be deallocated outside */
    memcpy(*outbuf, inbuf, inlen);

    /* Private data that was passed at the time of endpoint creation */
    uint32_t *priv = (uint32_t *) priv_data;
    if (priv) {
        printf("Private data : %lu", *priv);
    }

    ESP_LOGI("ouchat", "recived : %s", *outbuf);

    return ESP_OK;
}

void ouchat_start_protocomm() {
    //Protocomm initialization
    protocomm_t *ptcm = protocomm_new();
    protocomm_ble_name_uuid_t lookup_table[] = {
            {"prov-session",  0x0001},
            {"ouchat-config", 0x0002},
            {"proto-ver",     0x0003},
    };

    //Define the device name for the ble
    char ouchat_device_name[13];
    get_device_name(ouchat_device_name,sizeof ouchat_device_name);


    protocomm_ble_config_t config = DEFAULT_OUCHAT_PROTOCOMM_CONFIG

    strcpy(config.device_name,ouchat_device_name);

    protocomm_security2_params_t sec2_params = {};

    ouchat_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len);
    ouchat_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len);

    protocomm_ble_start(ptcm, &config);

    char *version_str = "{\"prov\":{\"ver\":\"v1.1\",\"sec_ver\":2,\"cap\":[]}}";
    protocomm_set_version(ptcm, "proto-ver", version_str);

    protocomm_set_security(ptcm, "prov-session", &protocomm_security2, &sec2_params);

    static uint32_t priv_data = 1234;
    protocomm_add_endpoint(ptcm, "ouchat-config", echo_req_handler, (void *) &priv_data);
}