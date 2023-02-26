idf_component_register(SRCS "ouchat_processing.c" "ouchat_utils.c" "ouchat_api.c" "ouchat_wifi_prov.c" "ouchat_wifi.c"
        INCLUDE_DIRS "include"
        REQUIRES "vl53l5cx" "esp_timer" "nvs_flash" "esp-tls" "mbedtls"  "log" "json" "bt" "wifi_provisioning" "esp_wifi" "freertos"
        EMBED_TXTFILES "ouchat_api_cert.pem")