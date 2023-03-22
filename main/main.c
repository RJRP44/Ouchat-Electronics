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
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "ouchat_wifi_prov.h"
#include "ouchat_wifi.h"
#include "led_strip.h"
#include "ouchat_led.h"

static const char *TAG = "Ouchat-Main";

#define I2C_SDA_NUM                      1
#define I2C_SCL_NUM                      2
#define I2C_CLK_SPEED                    1000000
#define I2C_TIMEOUT                      0x0000001FU
#define I2C_TX_BUF                       0
#define I2C_RX_BUF                       0

led_strip_handle_t led_strip;
rgb_color current_color;

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

 static void ouchat_event_handler(double_t x, double_t y, area_t start, area_t end){
     if(end.left_center.y == 0 && start.center.y >= 4.5){

         ouchat_animate(led_strip,SLIDE,5000, &current_color,(rgb_color){27,0,0});

         printf("Fast Outside\n");
         TaskHandle_t xHandle = NULL;

         xTaskCreate(https_request_task, "https_get_task", 8192, ( void * ) 0, 5, &xHandle);
         configASSERT( xHandle );
     }else if(y >= 4.5){
        if(end.center.y <= 2 ){

            ouchat_animate(led_strip,SLIDE,5000, &current_color,(rgb_color){27,0,0});

            printf("Outside\n");
            TaskHandle_t xHandle = NULL;

            xTaskCreate(https_request_task, "https_get_task", 8192, ( void * ) 0, 5, &xHandle);
            configASSERT( xHandle );

        }else{
            printf("Fake Outside\n");
        }
    }else if(y <= -4.5){
        if(start.center.y <= 2 ){

            ouchat_animate(led_strip,SLIDE,5000, &current_color,(rgb_color){8,27,0});

            printf("Inside\n");
            TaskHandle_t xHandle = NULL;

            xTaskCreate(https_request_task, "https_get_task", 8192, ( void * ) 1, 5, &xHandle);
            configASSERT( xHandle );
        }else{
           printf("Fake Inside\n");
        }
    }
}

_Noreturn void app_main(void) {

    esp_err_t nvs_ret = nvs_flash_init();

    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();

    esp_event_loop_create_default();

    ouchat_wifi_register_events();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //LED strip initialization
    led_strip_config_t strip_config = DEFAULT_OUCHAT_LED_STRIP_CONFIG;
    led_strip_rmt_config_t rmt_config = DEFAULT_OUCHAT_RMT_CONFIG;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    led_strip_clear(led_strip);

    current_color = (rgb_color){0,0,0};

    ouchat_animate(led_strip,SLIDE,5000, &current_color,(rgb_color){20,20,20});

    ouchat_init_provisioning();

    if(!ouchat_is_provisioned()){

        ESP_LOGI(TAG, "Starting provisioning...");

        ouchat_start_provisioning();

        ouchat_animate(led_strip,SLIDE,5000, &current_color,(rgb_color){0,11,27});

    } else {

        ESP_LOGI(TAG, "Already provisioned, freeing memory");

        //Free provisioning memory
        ouchat_deinit_provisioning();

        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
    }

    ouchat_wifi_register_handlers();

    //Initialize the i2c bus
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    uint8_t status, isAlive, isReady;

    //Define the i2c address and port
    VL53L5CX_Configuration *configuration = malloc(sizeof *configuration);
    configuration->platform.port = I2C_NUM_1;
    configuration->platform.address = 0x52;

      //Wakeup the sensor
      status = vl53l5cx_is_alive(configuration, &isAlive);
      if (!isAlive || status) {
         ESP_LOGI(TAG,"VL53L5CX not detected at requested address");
          while (1);
      }

      status = vl53l5cx_init(configuration);
      if (status) {
          ESP_LOGI(TAG,"VL53L5CX ULD Loading failed");
          while (1);
      }

      printf("VL53L5CX ULD ready ! (Version : %s)\n", VL53L5CX_API_REVISION);

      status = vl53l5cx_set_resolution(configuration, VL53L5CX_RESOLUTION_8X8);
      if (status) {
          printf("vl53l5cx_set_resolution failed, status %u\n", status);
          while (1);
      }

      status = vl53l5cx_set_ranging_frequency_hz(configuration, 15);
      if (status) {
          printf("vl53l5cx_set_ranging_frequency_hz failed, status %u\n", status);
          while (1);
      }

      uint8_t resolution;
      vl53l5cx_get_resolution(configuration, &resolution);
      printf("resolution : %d \n", (uint8_t) sqrt(resolution));
      vl53l5cx_start_ranging(configuration);

    VL53L5CX_ResultsData results;
    area_t output[16];
    //int16_t background[64];
    //uint8_t hasback = 0;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    int16_t context = 0;

    while (1) {

        memset(output,0,64);
        vl53l5cx_check_data_ready(configuration, &isReady);
        if (isReady) {
            vl53l5cx_get_ranging_data(configuration, &results);
            if(context == 0){
                ouchat_get_context(results.distance_mm, &context);
            }
            ouchat_handle_data(results.distance_mm,context,&ouchat_event_handler);
        }

        WaitMs(&(configuration->platform), 5);
    }
}