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

#define LOG_TAG "ouchat"

RTC_DATA_ATTR sensor_t sensor;

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

        ESP_LOGI(LOG_TAG, "Starting sensor calibration\n");
        calibrate_sensor(&sensor);
        ESP_LOGI(LOG_TAG, "Sensor calibrated !\n");

        //Stop the ranging to set the sensor in "sleep"
        sensor_update_config(&sensor, DEFAULT_VL53L8CX_LP_CONFIG);
        sensor_init_thresholds(&sensor);

        ESP_LOGI(LOG_TAG, "Entering deep sleep\n");

        //Add interrupt pin
        esp_deep_sleep_disable_rom_logging();
        esp_sleep_enable_ext0_wakeup(OUCHAT_SENSOR_DEFAULT_INT, 0);

        //Start the deep sleep
        esp_deep_sleep_start();
    }

    //Update and restart the sensor
    sensor_update_config(&sensor, DEFAULT_VL53L8CX_CONFIG);

    //Get the raw data
    VL53L8CX_ResultsData results;
    uint8_t status, ready = 0;

    //Wait for the sensor to restart ranging
    while (!ready) {
        WaitMs(&sensor.handle.platform, 5);
        vl53l8cx_check_data_ready(&sensor.handle, &ready);
    }

    process_init();
    uint16_t empty_frames = 0;

    while (empty_frames < 30) {
        status = vl53l8cx_check_data_ready(&sensor.handle, &ready);

        if (ready) {

            uint16_t raw_data[8][8];
            coord_t sensor_data[8][8];

            //Convert 1D data to grid
            vl53l8cx_get_ranging_data(&sensor.handle, &results);
            memcpy(raw_data, results.distance_mm, sizeof(raw_data));

            //Correct the data according to the calibration
            correct_sensor_data(&sensor, raw_data, sensor_data);

            status = process_data(sensor_data, sensor.calibration);

            if (empty_frames != 0 || status) {
                empty_frames = status == 0 ? 0 : empty_frames + status;
            }
        }
    }

    //Stop the ranging to set the sensor in "sleep"
    sensor_update_config(&sensor, DEFAULT_VL53L8CX_LP_CONFIG);
    sensor_init_thresholds(&sensor);

    ESP_LOGI(LOG_TAG, "Entering deep sleep\n");

    //Add interrupt pin
    esp_deep_sleep_disable_rom_logging();
    esp_sleep_enable_ext0_wakeup(OUCHAT_SENSOR_DEFAULT_INT, 0);

    //Start the deep sleep
    esp_deep_sleep_start();
}