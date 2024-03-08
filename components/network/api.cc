//
// Created by Romain on 25/02/2024.
//

#include <string>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include "api.h"

#define WEB_SERVER "v2.api.ouchat.fr"
#define SET_WEB_URL "https://" WEB_SERVER "/api/cat/set?key=" CONFIG_OUCHAT_KEY "&cat=" CONFIG_OUCHAT_CAT "&value="

extern const unsigned char ouchat_api_cert[] asm("_binary_ouchat_api_cert_pem_start");
EventGroupHandle_t api_flags;

esp_err_t init_api(){
    api_flags = xEventGroupCreate();

    return ESP_OK;
}

void api_set(void *args) {

    int16_t value = *static_cast<int16_t*>(args);

    xEventGroupSetBits(api_flags, REQUEST_FLAG);
    xEventGroupClearBits(api_flags, REQUEST_DONE_FLAG);

    std::string request = SET_WEB_URL + value;

    esp_http_client_config_t client_config = {
            .url = request.c_str(),
            .cert_pem = reinterpret_cast<const char*>(ouchat_api_cert)
    };

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(API_TAG, "HTTPS GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_close(client);

    xEventGroupClearBits(api_flags, REQUEST_FLAG);
    vTaskDelete(nullptr);
}