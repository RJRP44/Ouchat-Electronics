#include <sys/cdefs.h>
#include <stdio.h>
#include <math.h>
#include <esp_timer.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "vl53l8cx_api.h"
#include "ouchat_processing.h"
#include "ouchat_api.h"
#include <string.h>
#include <cJSON.h>
#include <esp_sleep.h>
#include <mbedtls/base64.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "ouchat_wifi.h"
#include "ouchat_led.h"
#include "ouchat_sensor.h"
#include "ouchat_logger.h"
#include "vl53l8cx_plugin_detection_thresholds.h"

static const char *TAG = "Ouchat-Main";

RTC_DATA_ATTR VL53L8CX_Configuration ouchat_sensor_configuration;
RTC_DATA_ATTR int16_t ouchat_sensor_context = 0;
RTC_DATA_ATTR int16_t ouchat_sensor_background[64];

ouchat_motion_threshold_config threshold_config = {
        .distance_min = 400,
        .distance_max = 800,
        .motion_threshold = 44
};

static void ouchat_event_handler(double_t x, double_t y, area_t start, area_t end) {

    if (end.left_center.y == 0 && start.center.y >= 4.5) {

        printf("Fast Outside\n");

        ouchat_api_status = REQUESTING_API;

        TaskHandle_t xHandle = NULL;

        xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 4, &xHandle);
        configASSERT(xHandle);
    } else if (y >= 4.5) {
        if (end.center.y <= 2) {

            printf("Outside\n");

            ouchat_api_status = REQUESTING_API;
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 4, &xHandle);
            configASSERT(xHandle);

        } else {
            printf("Fake Outside\n");
        }
    } else if (y <= -4.5) {
        if (start.center.y <= 2) {

            printf("Inside\n");

            ouchat_api_status = REQUESTING_API;
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 1, 4, &xHandle);
            configASSERT(xHandle);
        } else {
            printf("Fake Inside\n");
        }
    }
}

void app_main(void) {

    ouchat_api_status = WAITING_WIFI;

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

        //Power on sensor and init
        ouchat_sensor_configuration.platform.address = VL53L8CX_DEFAULT_I2C_ADDRESS;
        ouchat_sensor_configuration.platform.port = I2C_NUM_1;

        ouchat_sensor_config sensor_config = {
                .sensor_config = &ouchat_sensor_configuration,
                .ranging_mode = VL53L8CX_RANGING_MODE_CONTINUOUS,
                .frequency_hz = 15,
                .resolution = VL53L8CX_RESOLUTION_8X8
        };

        ouchat_init_sensor(sensor_config);

        //Get the context / environment of the scan
        vl53l8cx_start_ranging(&ouchat_sensor_configuration);

        uint8_t is_ready = 0;
        vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);

        //Wait for the sensor to start ranging
        while (!is_ready){
            WaitMs(&(ouchat_sensor_configuration.platform), 5);
            vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
        }

        //Get the raw data
        VL53L8CX_ResultsData results;
        vl53l8cx_get_ranging_data(&ouchat_sensor_configuration, &results);

        //Process it the get the approximate distance
        ouchat_get_context(results.distance_mm, &ouchat_sensor_context);

        memcpy(ouchat_sensor_background, results.distance_mm, sizeof results.distance_mm);

        printf("context distance : %i mm \n", ouchat_sensor_context);

        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < 8; ++k) {
                printf("%i ", results.distance_mm[8*j + k]);
            }
            printf("\n");
        }

        //Stop the ranging to set the sensor in "sleep"
        vl53l8cx_stop_ranging(&ouchat_sensor_configuration);

        sensor_config = (ouchat_sensor_config){
                .sensor_config = &ouchat_sensor_configuration,
                .ranging_mode = VL53L8CX_RANGING_MODE_AUTONOMOUS,
                .frequency_hz = 5,
                .resolution = VL53L8CX_RESOLUTION_8X8,
                .integration_time = 10,
        };

        ouchat_lp_sensor(sensor_config, threshold_config, &ouchat_sensor_background[0]);
        vl53l8cx_start_ranging(&ouchat_sensor_configuration);

        esp_deep_sleep_disable_rom_logging();

        esp_sleep_enable_ext0_wakeup(GPIO_NUM_3,0);
        esp_deep_sleep_start();
    }

    //By using Interrupt autostop, the frame measurements are aborted when an interrupt is triggered
    ouchat_sensor_config sensor_config = (ouchat_sensor_config){
            .sensor_config = &ouchat_sensor_configuration,
            .ranging_mode = VL53L8CX_RANGING_MODE_AUTONOMOUS,
            .frequency_hz = 5,
            .resolution = VL53L8CX_RESOLUTION_8X8,
            .integration_time = 10,
    };

    vl53l8cx_stop_ranging(&ouchat_sensor_configuration);

    vl53l8cx_set_ranging_mode(&ouchat_sensor_configuration, VL53L8CX_RANGING_MODE_CONTINUOUS);
    vl53l8cx_set_ranging_frequency_hz(&ouchat_sensor_configuration, 15);
    vl53l8cx_set_detection_thresholds_auto_stop(&ouchat_sensor_configuration, 0);
    vl53l8cx_start_ranging(&ouchat_sensor_configuration);

    //Get the raw data
    VL53L8CX_ResultsData results;
    uint8_t is_ready = 0;
    uint8_t status;

    //Wait for the sensor to restart ranging
    while (!is_ready){
        WaitMs(&(ouchat_sensor_configuration.platform), 5);
        vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
    }

    ouchat_processing_wakeup();


#if CONFIG_OUCHAT_DEBUG_LOGGER

    ouchat_logger_init();

    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(ouchat_logger, "ouchat_log", 16384,&ouchat_sensor_context, 5, &xHandle, 1);
    configASSERT(xHandle);
#else

    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(ouchat_wifi_wakeup, "ouchat_wifi", 16384,NULL, 5, &xHandle, 1);
    configASSERT(xHandle);

#endif


    uint16_t empty_frames = 0;

    while(empty_frames < 30)
    {
        status = vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);

        if(is_ready)
        {
            vl53l8cx_get_ranging_data(&ouchat_sensor_configuration, &results);
            status = ouchat_handle_data(results.distance_mm,ouchat_sensor_context,&ouchat_event_handler);

#if CONFIG_OUCHAT_DEBUG_LOGGER

            size_t outlen;
            unsigned char output[190];

            mbedtls_base64_encode(output, 190, &outlen, (const unsigned char *) results.distance_mm, sizeof (results.distance_mm));

            ouchat_log(output, outlen);
            printf("%s\n", output);
#endif

            if(empty_frames != 0 || status){
                empty_frames = status == 0 ? 0 : empty_frames + status;
            }
        }
    }

    //Wait fot the api request to be compete
    while (ouchat_api_status == REQUESTING_API){
        WaitMs(&(ouchat_sensor_configuration.platform), 20);
    }

    WaitMs(&(ouchat_sensor_configuration.platform), 2500);

#if CONFIG_OUCHAT_DEBUG_LOGGER
    //If logging is enabled end the connection
    ouchat_stop_logger();
#endif

    vl53l8cx_stop_ranging(&ouchat_sensor_configuration);

    ouchat_lp_sensor(sensor_config, threshold_config, &ouchat_sensor_background[0]);
    vl53l8cx_start_ranging(&ouchat_sensor_configuration);

    esp_deep_sleep_disable_rom_logging();

    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_NUM_3,0));
    esp_deep_sleep_start();

}