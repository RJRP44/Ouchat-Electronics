idf_component_register(
        SRCS logger.cc api.cc wifi.cc
        INCLUDE_DIRS include
        REQUIRES nvs_flash esp_wifi esp_event cjson mbedtls utils esp_http_client
        EMBED_TXTFILES ouchat_api_cert.pem
)