idf_component_register(
        SRCS logger.c api.c wifi.c
        INCLUDE_DIRS include
        REQUIRES nvs_flash esp_wifi esp_event json mbedtls utils esp_http_client
        EMBED_TXTFILES ouchat_api_cert.pem
)