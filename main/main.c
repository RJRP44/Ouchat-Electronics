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

RTC_DATA_ATTR VL53L5CX_Configuration ouchat_sensor_configuration;
RTC_DATA_ATTR int16_t ouchat_sensor_context = 0;
RTC_DATA_ATTR int16_t ouchat_sensor_background[64];

ouchat_motion_threshold_config threshold_config = {
        .distance_min = 400,
        .distance_max = 800,
        .motion_threshold = 44
};

static void ouchat_event_handler(double_t x, double_t y, area_t start, area_t end) {
    if (end.left_center.y == 0 && start.center.y >= 4.5) {

        ouchat_wifi_wakeup();

        printf("Fast Outside\n");
        TaskHandle_t xHandle = NULL;

        xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 5, &xHandle);
        configASSERT(xHandle);
    } else if (y >= 4.5) {
        if (end.center.y <= 2) {

            ouchat_wifi_wakeup();

            printf("Outside\n");
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 5, &xHandle);
            configASSERT(xHandle);

        } else {
            printf("Fake Outside\n");
        }
    } else if (y <= -4.5) {
        if (start.center.y <= 2) {

            ouchat_wifi_wakeup();

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
        esp_wifi_init(&cfg);

        ouchat_init_provisioning();

        if(!ouchat_is_provisioned()){

            printf("Starting provisioning...");

            ouchat_start_provisioning();

        } else {

            printf("Already provisioned, freeing memory");

            //Free provisioning memory
            ouchat_deinit_provisioning();

            //ouchat_start_protocomm();

            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_start();
        }

        ouchat_wifi_register_handlers();

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

        memcpy(ouchat_sensor_background, results.distance_mm, sizeof results.distance_mm);

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

        ouchat_lp_sensor(sensor_config, threshold_config, &ouchat_sensor_background[0]);
        vl53l5cx_start_ranging(&ouchat_sensor_configuration);

        esp_deep_sleep_disable_rom_logging();

        esp_sleep_enable_ext0_wakeup(GPIO_NUM_17,0);
        esp_deep_sleep_start();
        return;
    }

    uint8_t is_ready = 0;
    uint8_t status;

    //Wait for the sensor to restart ranging
    while (!is_ready){
        WaitMs(&(ouchat_sensor_configuration.platform), 5);
        vl53l5cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
    }

    ouchat_processing_wakeup();

    //Get the raw data
    VL53L5CX_ResultsData results;
    vl53l5cx_get_ranging_data(&ouchat_sensor_configuration, &results);

    status = ouchat_handle_data(results.distance_mm,ouchat_sensor_context,&ouchat_event_handler);

    area_t wakeup_frame[16];

    memcpy(wakeup_frame, ouchat_areas, sizeof(ouchat_areas));

    while (!is_ready){
        WaitMs(&(ouchat_sensor_configuration.platform), 5);
        vl53l5cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
    }

    vl53l5cx_get_ranging_data(&ouchat_sensor_configuration, &results);

    status = ouchat_handle_data(results.distance_mm,ouchat_sensor_context,&ouchat_event_handler);



    for (int i = 0; i < 16; ++i) {
        if(wakeup_frame[i].top_left != C_TO_1D(16, 16) && ouchat_areas[i].top_left != C_TO_1D(16,16)){
            ouchat_process_trajectory_points(wakeup_frame[i],ouchat_areas[i], i);
        }
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

    ouchat_lp_sensor(sensor_config, threshold_config,&ouchat_sensor_background[0]);
    vl53l5cx_start_ranging(&ouchat_sensor_configuration);

    esp_deep_sleep_disable_rom_logging();

    esp_sleep_enable_ext0_wakeup(GPIO_NUM_17,0);
    esp_deep_sleep_start();
}