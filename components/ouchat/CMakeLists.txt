idf_component_register(SRCS "ouchat_processing.c" "ouchat_utils.c" "ouchat_api.c" "ouchat_wifi.c" "ouchat_led.c" "ouchat_sensor.c" "ouchat_logger.c"       INCLUDE_DIRS "include"
        REQUIRES "vl53l8cx" "esp_timer" "nvs_flash" "esp-tls" "mbedtls"  "log" "json" "esp_wifi" "freertos" "led_strip" "freertos" "lwip"
        EMBED_TXTFILES "ouchat_api_cert.pem")