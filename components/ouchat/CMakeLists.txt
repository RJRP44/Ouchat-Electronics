idf_component_register(SRCS "ouchat_processing.c" "ouchat_utils.c" "ouchat_api.c" "ouchat_bt.c"
        INCLUDE_DIRS "include"
        REQUIRES "vl53l5cx" "esp_timer" "nvs_flash" "esp-tls" "mbedtls"  "log" "bt" "json"
        EMBED_TXTFILES "ouchat_api_cert.pem")