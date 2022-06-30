#include <sys/cdefs.h>
#include <stdio.h>
#include <math.h>
#include <esp_timer.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "vl53l5cx_api.h"
#include "ouchat_processing.h"
#include "ouchat_api.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

static const char *TAG = "v53l5cx_lib";

#define I2C_SDA_NUM                      21
#define I2C_SCL_NUM                      22
#define I2C_CLK_SPEED                    1000000
#define I2C_TIMEOUT                      400000
#define I2C_TX_BUF                       0
#define I2C_RX_BUF                       0

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK



static esp_err_t i2c_master_init(void) {

    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_SDA_NUM,
            .scl_io_num = I2C_SCL_NUM,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_CLK_SPEED,
    };

    i2c_param_config(I2C_NUM_1, &conf);
    i2c_set_timeout(I2C_NUM_1, I2C_TIMEOUT);

    return i2c_driver_install(I2C_NUM_1, conf.mode, I2C_RX_BUF, I2C_TX_BUF, 0);
}

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

void ouchat_prepare_data(int16_t pInt[64], int16_t pInt1[64], void (*pFunction)(uint8_t, uint8_t));

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, BIT1);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, BIT0);
    }
}

 static void ouchat_event_handler(double_t x, double_t y){
    if(y > 6.00){
        printf("Outside\n");
        TaskHandle_t xHandle = NULL;
        uint8_t args = 0;

        xTaskCreate(https_request_task, "https_get_task", 8192, (void*)&args, 5, &xHandle);
        configASSERT( xHandle );
    }else if(y < -6.00){
        printf("Inside\n");
        TaskHandle_t xHandle = NULL;
        uint8_t args_inside = 1;

        xTaskCreate(https_request_task, "https_get_task", 8192, (void*)&args_inside, 5, &xHandle);
        configASSERT( xHandle );
    }
}

_Noreturn void app_main(void) {

    //Initialize the i2c bus
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    uint8_t status, isAlive, i , isReady;

    //Define the i2c address and port
    VL53L5CX_Configuration *configuration = malloc(sizeof *configuration);
    configuration->platform.port = I2C_NUM_1;
    configuration->platform.address = 0x52;

    //Wakeup the sensor
    status = vl53l5cx_is_alive(configuration, &isAlive);
    if (!isAlive || status) {
        printf("VL53L5CX not detected at requested address\n");
        while (1);
    }

    /* (Mandatory) Init VL53L5CX sensor */
    status = vl53l5cx_init(configuration);
    if (status) {
        printf("VL53L5CX ULD Loading failed\n");
        while (1);
    }

    printf("VL53L5CX ULD ready ! (Version : %s)\n", VL53L5CX_API_REVISION);

    status = vl53l5cx_set_resolution(configuration, VL53L5CX_RESOLUTION_8X8);
    if (status) {
        printf("vl53l5cx_set_resolution failed, status %u\n", status);
        while (1);
    }

    status = vl53l5cx_set_ranging_frequency_hz(configuration, 15);
    if(status)
    {
        printf("vl53l5cx_set_ranging_frequency_hz failed, status %u\n", status);
        while (1);
    }

    uint8_t resolution;
    vl53l5cx_get_resolution(configuration,&resolution);
    printf("resolution : %d \n", (uint8_t) sqrt(resolution));
    status = vl53l5cx_start_ranging(configuration);

    VL53L5CX_ResultsData results;
    area_t output[16];
    area_t *p_output = &output[0];
    int16_t background[64];
    uint8_t hasback = 0;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));


    wifi_config_t wifi_config = {
            .sta = {
                    .ssid = CONFIG_ESP_WIFI_SSID,
                    .password = CONFIG_ESP_WIFI_PASSWORD,
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                     * However these modes are deprecated and not advisable to be used. Incase your Access point
                     * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           BIT0 | BIT1,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & BIT0) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else if (bits & BIT1) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    //https_request_task(NULL);


    while (1) {

        memset(output,0,64);
        status = vl53l5cx_check_data_ready(configuration, &isReady);
        if (isReady) {
            vl53l5cx_get_ranging_data(configuration, &results);
            if(hasback == 0){
                hasback = 1;
                for (int j = 0; j < 64; ++j) {
                    background[j] = results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE*j];
                }
            }else{
                ouchat_handle_data(results.distance_mm,background,&ouchat_event_handler);
            }
        }

        WaitMs(&(configuration->platform), 5);
    }


}