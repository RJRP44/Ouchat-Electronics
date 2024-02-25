#include <esp_log.h>
#include <driver/i2c.h>
#include <sdkconfig.h>
#include <vl53l8cx_api.h>
#include <string.h>
#include <esp_sleep.h>
#include <nvs_flash.h>
#include <sensor.h>
#include <calibrator.h>
#include <data_processor.h>
#include <logger.h>
#include <mbedtls/base64.h>

#define LOG_TAG "ouchat"

RTC_DATA_ATTR sensor_t sensor;

void side_tasks(void *arg){

    //Start all task without interrupting the reads
#if CONFIG_OUCHAT_DEBUG_LOGGER
    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(tcp_logger_task, "ouchat_logger", 16384, &sensor.calibration, 5, &xHandle, 1);
    configASSERT(xHandle);
#else
#endif

    vTaskDelete(NULL);
}

void app_main(void) {

    //Get the wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    //Apply, init the configuration to the bus
    init_i2c(I2C_NUM_1, DEFAULT_I2C_CONFIG);

    //First start
    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {

        //Power on sensor and init
        sensor.handle.platform.address = VL53L8CX_DEFAULT_I2C_ADDRESS;
        sensor.handle.platform.port = I2C_NUM_1;

        //Set the default 15Hz, 8x8 continuous config
        sensor.config = DEFAULT_VL53L8CX_CONFIG;

        sensor_init(&sensor);

        //Calibrate the sensor
        vl53l8cx_start_ranging(&sensor.handle);

        ESP_LOGI(LOG_TAG, "Starting sensor calibration");
        calibrate_sensor(&sensor);
        ESP_LOGI(LOG_TAG, "Sensor calibrated on %d points !", sensor.calibration.inliers);

        //Init motion detection based on the previous calibration
        init_motion_indicator(&sensor);

        //Reset the trigger saved by the bistable 555 circuit
        reset_sensor_trigger();

        //Stop the ranging to set the sensor in "sleep"
        sensor_update_config(&sensor, DEFAULT_VL53L8CX_LP_CONFIG);
        sensor_init_thresholds(&sensor);

        ESP_LOGI(LOG_TAG, "Entering deep sleep");

        //Add interrupt pin
        esp_deep_sleep_disable_rom_logging();
        esp_sleep_enable_ext0_wakeup(OUCHAT_SENSOR_DEFAULT_INT, 0);

        //Start the deep sleep
        esp_deep_sleep_start();
    }

#if CONFIG_OUCHAT_DEBUG_LOGGER
    init_tcp_logger();
#endif

    //Update and restart the sensor
    sensor_update_config(&sensor, DEFAULT_VL53L8CX_CONFIG);

    //Get the raw data
    VL53L8CX_ResultsData results;
    uint8_t status, ready = 0;

    process_init();
    uint16_t empty_frames = 0;
    bool side_task = false;

    while (empty_frames < 30) {
        vl53l8cx_check_data_ready(&sensor.handle, &ready);

        if (ready) {

            uint16_t raw_data[8][8];
            coord_t sensor_data[8][8];

            //Convert 1D data to grid
            vl53l8cx_get_ranging_data(&sensor.handle, &results);
            memcpy(raw_data, results.distance_mm, sizeof(raw_data));

            //Correct the data according to the calibration
            correct_sensor_data(&sensor, raw_data, sensor_data);

            status = process_data(sensor_data, sensor.calibration);


#if CONFIG_OUCHAT_DEBUG_LOGGER

            //Use base 64 to save the raw data
            size_t length;
            unsigned char output[190];

            mbedtls_base64_encode(output, 190, &length, (const unsigned char *) results.distance_mm, sizeof (results.distance_mm));
            tcp_log(output);

#endif

            if (empty_frames != 0 || status) {
                empty_frames = status == 0 ? 0 : empty_frames + status;
            }

        }else if(!side_task){

            //Start the side task without interrupting the reads
            TaskHandle_t xHandle = NULL;
            xTaskCreatePinnedToCore(side_tasks, "ouchat_side_tasks", 4096, NULL, 3, &xHandle, 1);
            configASSERT(xHandle);

            side_task = true;
        }

        WaitMs(&sensor.handle.platform, 5);
    }

#if CONFIG_OUCHAT_DEBUG_LOGGER

    //Stop the logger before sleep
    stop_tcp_logger();

#endif

    //Reset the trigger saved by the bistable 555 circuit
    reset_sensor_trigger();

    //Add interrupt pin
    esp_deep_sleep_disable_rom_logging();
    esp_sleep_enable_ext0_wakeup(OUCHAT_SENSOR_DEFAULT_INT, 0);

    //Stop the ranging to set the sensor in "sleep"
    sensor_update_config(&sensor, DEFAULT_VL53L8CX_LP_CONFIG);
    sensor_init_thresholds(&sensor);

    ESP_LOGI(LOG_TAG, "Entering deep sleep");

    //Start the deep sleep
    esp_deep_sleep_start();
}