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
#include <cJSON.h>
#include <esp_sleep.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "ouchat_wifi_prov.h"
#include "ouchat_wifi.h"
#include "led_strip.h"
#include "ouchat_led.h"
#include "ouchat_protocomm.h"
#include "ouchat_sensor.h"

static const char *TAG = "Ouchat-Main";

led_strip_handle_t led_strip;
rgb_color current_color;
rgb_color current_error;

RTC_DATA_ATTR VL53L5CX_Configuration ouchat_sensor_configuration;
RTC_DATA_ATTR int16_t ouchat_sensor_context = 0;

ouchat_motion_threshold_config threshold_config = {
        .distance_min = 400,
        .distance_max = 800,
        .motion_threshold = 44
};

static void ouchat_event_handler(double_t x, double_t y, area_t start, area_t end) {
    if (end.left_center.y == 0 && start.center.y >= 4.5) {

        //ouchat_animate(led_strip, SLIDE, RIGHT, 5000, &current_color, (rgb_color) {7, 0, 0});

        printf("Fast Outside\n");
        TaskHandle_t xHandle = NULL;

        xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 5, &xHandle);
        configASSERT(xHandle);
    } else if (y >= 4.5) {
        if (end.center.y <= 2) {

            //ouchat_animate(led_strip, SLIDE, RIGHT, 5000, &current_color, (rgb_color) {7, 0, 0});

            printf("Outside\n");
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 5, &xHandle);
            configASSERT(xHandle);

        } else {
            printf("Fake Outside\n");
        }
    } else if (y <= -4.5) {
        if (start.center.y <= 2) {

            //ouchat_animate(led_strip, SLIDE, LEFT, 5000, &current_color, (rgb_color) {2, 7, 0});

            printf("Inside\n");
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 1, 5, &xHandle);
            configASSERT(xHandle);
        } else {
            printf("Fake Inside\n");
        }
    }
}

void app_main(void) {

    //Get the wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    //Define the i2c bus configuration
    i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = OUCHAT_SENSOR_DEFAULT_SDA,
            .scl_io_num = OUCHAT_SENSOR_DEFAULT_SCL,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_DEFAULT_CLK_SPEED,
    };

    //Apply, init the configuration to the bus
    ouchat_init_i2c(I2C_NUM_1, i2c_config);

    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0){

        //Power on sensor and init
        ouchat_sensor_configuration.platform.address = VL53L5CX_DEFAULT_I2C_ADDRESS;
        ouchat_sensor_configuration.platform.port = I2C_NUM_1;

        ouchat_sensor_config sensor_config = {
                .sensor_config = &ouchat_sensor_configuration,
                .ranging_mode = VL53L5CX_RANGING_MODE_CONTINUOUS,
                .frequency_hz = 15,
                .resolution = VL53L5CX_RESOLUTION_8X8
        };

        ouchat_init_sensor(sensor_config);

        //Get the context / environment of the scan
        vl53l5cx_start_ranging(&ouchat_sensor_configuration);

        uint8_t is_ready = 0;
        vl53l5cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);

        //Wait for the sensor to start ranging
        while (!is_ready){
            WaitMs(&(ouchat_sensor_configuration.platform), 5);
            vl53l5cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
        }

        //Get the raw data
        VL53L5CX_ResultsData results;
        vl53l5cx_get_ranging_data(&ouchat_sensor_configuration, &results);

        //Process it the get the approximate distance
        ouchat_get_context(results.distance_mm, &ouchat_sensor_context);

        printf("context distance : %i mm \n", ouchat_sensor_context);

        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < 8; ++k) {
                printf("%i ", results.distance_mm[8*j + k]);
            }
            printf("\n");
        }

        //Stop the ranging to set the sensor in "sleep"
        vl53l5cx_stop_ranging(&ouchat_sensor_configuration);

        sensor_config = (ouchat_sensor_config){
                .sensor_config = &ouchat_sensor_configuration,
                .ranging_mode = VL53L5CX_RANGING_MODE_AUTONOMOUS,
                .frequency_hz = 5,
                .resolution = VL53L5CX_RESOLUTION_8X8,
                .integration_time = 10,
        };

        ouchat_lp_sensor(sensor_config, threshold_config);
        vl53l5cx_start_ranging(&ouchat_sensor_configuration);

        esp_deep_sleep_disable_rom_logging();

        esp_sleep_enable_ext0_wakeup(GPIO_NUM_17,0);
        esp_deep_sleep_start();
        return;
    }

    ouchat_sensor_config sensor_config = (ouchat_sensor_config){
            .sensor_config = &ouchat_sensor_configuration,
            .ranging_mode = VL53L5CX_RANGING_MODE_AUTONOMOUS,
            .frequency_hz = 5,
            .resolution = VL53L5CX_RESOLUTION_8X8,
            .integration_time = 10,
    };

    vl53l5cx_stop_ranging(&ouchat_sensor_configuration);

    vl53l5cx_set_ranging_mode(&ouchat_sensor_configuration, VL53L5CX_RANGING_MODE_CONTINUOUS);
    vl53l5cx_set_ranging_frequency_hz(&ouchat_sensor_configuration, 15);

    vl53l5cx_start_ranging(&ouchat_sensor_configuration);

    uint16_t empty_frames = 0;
    uint8_t is_ready = 0;
    uint8_t status;
    VL53L5CX_ResultsData results;

    ouchat_processing_wakeup();
    while(empty_frames < 30)
    {
        status = vl53l5cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
        if(is_ready)
        {
            vl53l5cx_get_ranging_data(&ouchat_sensor_configuration, &results);
            status = ouchat_handle_data(results.distance_mm,ouchat_sensor_context,&ouchat_event_handler);

            if(empty_frames != 0 || status){
                empty_frames = status == 0 ? 0 : empty_frames + status;
            }

        }

        /* Wait a few ms to avoid too high polling (function in platform
         * file, not in API) */
        WaitMs(&(ouchat_sensor_configuration.platform), 5);
    }

    vl53l5cx_stop_ranging(&ouchat_sensor_configuration);

    ouchat_lp_sensor(sensor_config, threshold_config);
    vl53l5cx_start_ranging(&ouchat_sensor_configuration);

    esp_deep_sleep_disable_rom_logging();

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_17,0);
    esp_deep_sleep_start();
    /*

    current_error = (rgb_color){0,0,0};

    esp_err_t nvs_ret = nvs_flash_init();

    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();

    esp_event_loop_create_default();

    ouchat_wifi_register_events(&led_strip);

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //LED strip initialization
    led_strip_config_t strip_config = DEFAULT_OUCHAT_LED_STRIP_CONFIG;
    led_strip_rmt_config_t rmt_config = DEFAULT_OUCHAT_RMT_CONFIG;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    led_strip_clear(led_strip);

    current_color = (rgb_color) {0, 0, 0};

    ouchat_animate(led_strip, SLIDE, RIGHT, 5000, &current_color, (rgb_color) {5, 5, 5});

    ouchat_init_provisioning();

    if(!ouchat_is_provisioned()){

        ESP_LOGI(TAG, "Starting provisioning...");

        ouchat_start_provisioning();

        ouchat_animate(led_strip,SLIDE, RIGHT,5000, &current_color,(rgb_color){0,3,7});

    } else {

        ESP_LOGI(TAG, "Already provisioned, freeing memory");

        //Free provisioning memory
        ouchat_deinit_provisioning();

        ouchat_start_protocomm();

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
          ouchat_error(led_strip,3000,&current_error,(rgb_color){12,0,14});
          return;
      }

      status = vl53l5cx_init(configuration);
      if (status) {
          ESP_LOGI(TAG,"VL53L5CX ULD Loading failed");
          return;
      }

      printf("VL53L5CX ULD ready ! (Version : %s)\n", VL53L5CX_API_REVISION);

      status = vl53l5cx_set_resolution(configuration, VL53L5CX_RESOLUTION_8X8);
      if (status) {
          printf("vl53l5cx_set_resolution failed, status %u\n", status);
          return;
      }

      status = vl53l5cx_set_ranging_frequency_hz(configuration, 15);
      if (status) {
          printf("vl53l5cx_set_ranging_frequency_hz failed, status %u\n", status);
          return;
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
            }else{
                ouchat_handle_data(results.distance_mm,context,&ouchat_event_handler);
            }
        }

        WaitMs(&(configuration->platform), 5);
    }*/
}