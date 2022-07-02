//
// Created by RJRP on 29/06/2022.
//

#include "ouchat_api.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "lwip/err.h"
#include "lwip/sockets.h"

#include "esp_tls.h"
#include "sdkconfig.h"
#include "esp_crt_bundle.h"

#define WEB_SERVER "v2.api.ouchat.fr"
#define WEB_URL "https://v2.api.ouchat.fr/api/cat/set?key=" CONFIG_OUCHAT_KEY "&cat=" CONFIG_OUCHAT_CAT "&value="

static const char *TAG = "ouchat-api";

static char OUCHAT_API_REQUEST[146];

/* Root cert for v2.api.ouchat.fr, taken from ouchat_api_cert.pem
   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect v2.api.ouchat.com:443 </dev/null
   The CA root cert is the last cert given in the chain of certs.
*/

extern const uint8_t server_root_cert_pem_start[] asm("_binary_ouchat_api_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_ouchat_api_cert_pem_end");


static void https_get_request(esp_tls_cfg_t cfg, const char *WEB_SERVER_URL, const char *REQUEST)
{
    char buf[512];
    int ret, len;

    esp_tls_t *tls = esp_tls_init();
    if (!tls) {
        ESP_LOGE(TAG, "Failed to allocate esp_tls handle!");
        return;
    }

    if (esp_tls_conn_http_new_sync(WEB_SERVER_URL, &cfg, tls) == 1) {
        ESP_LOGI(TAG, "Connection established...");
    } else {
        ESP_LOGE(TAG, "Connection failed...");
        goto cleanup;
    }

    size_t written_bytes = 0;
    do {
        ret = esp_tls_conn_write(tls,
                                 REQUEST + written_bytes,
                                 strlen(REQUEST) - written_bytes);
        if (ret >= 0) {
            ESP_LOGI(TAG, "%d bytes written", ret);
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned: [0x%02X](%s)", ret, esp_err_to_name(ret));
            goto cleanup;
        }
    } while (written_bytes < strlen(REQUEST));

    ESP_LOGI(TAG, "Reading HTTP response...");
    do {
        len = sizeof(buf) - 1;
        memset(buf, 0x00, sizeof(buf));
        ret = esp_tls_conn_read(tls, (char *)buf, len);

        if (ret == ESP_TLS_ERR_SSL_WANT_WRITE  || ret == ESP_TLS_ERR_SSL_WANT_READ) {
            continue;
        } else if (ret < 0) {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned [-0x%02X](%s)", -ret, esp_err_to_name(ret));
            break;
        } else if (ret == 0) {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        len = ret;
        ESP_LOGD(TAG, "%d bytes read", len);
        /* Print response directly to stdout as it is read */
        for (int i = 0; i < len; i++) {
            putchar(buf[i]);
        }
        putchar('\n'); // JSON output doesn't have a newline at end
    } while (1);

    cleanup:
    esp_tls_conn_destroy(tls);
}

static void https_get_request_using_crt_bundle(void)
{
    ESP_LOGI(TAG, "https_request using crt bundle");
    esp_tls_cfg_t cfg = {
            .crt_bundle_attach = esp_crt_bundle_attach,
    };
    https_get_request(cfg, WEB_URL, OUCHAT_API_REQUEST);
}


void https_request_task(void *pvparameters)
{
    strcpy(OUCHAT_API_REQUEST,"GET " WEB_URL);
    printf("https : %d",*(uint8_t *)pvparameters);

    if((*(uint8_t *)pvparameters) == 5){
        strcat(OUCHAT_API_REQUEST, "1");
    }else{
        strcat(OUCHAT_API_REQUEST, "0");
    }
    strcat(OUCHAT_API_REQUEST," HTTP/1.1\r\n"
                               "Host: " WEB_SERVER "\r\n"
                               "User-Agent: esp-idf/1.0 esp32\r\n"
                               "\r\n");
    ESP_LOGI(TAG, "%s", OUCHAT_API_REQUEST );
    ESP_LOGI(TAG, "Start https_request example");

    https_get_request_using_crt_bundle();

    ESP_LOGI(TAG, "Minimum free heap size: %d bytes", esp_get_minimum_free_heap_size());
    ESP_LOGI(TAG, "Finish https_request example");
    vTaskDelete(NULL);
}