#include <ai.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <sdkconfig.h>
#include <vl53l8cx_api.h>
#include <esp_sleep.h>
#include <sensor.h>
#include <calibrator.h>
#include <data_processor.h>
#include <logger.h>
#include <mbedtls/base64.h>
#include <api.h>
#include <leds.h>
#include <model.h>

#define LOG_TAG "ouchat"

RTC_DATA_ATTR sensor_t sensor;

using namespace ouchat;

void side_tasks(void *arg){

    //Start all task without interrupting the reads
#if CONFIG_OUCHAT_DEBUG_LOGGER
    TaskHandle_t xHandle = nullptr;

    xTaskCreatePinnedToCore(tcp_logger_task, "ouchat_logger", 16384, &sensor.calibration, 5, &xHandle, 1);
    configASSERT(xHandle);
#else
    bool status = 0;
    wifi_init(&status);

    //If not connected to the Wi-Fi cancel
    if (!status){
        ESP_LOGE(LOG_TAG, "Not connected to Wi-Fi...");
    }
#endif

    init_api();
    init_leds();

    ai::interpreter::init(ai::model);

    vTaskDelete(nullptr);
}

extern "C" void app_main(void) {

    //Get the wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    //Apply the configuration to the i²c bus
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t i2c_config = DEFAULT_I2C_BUS_CONFIG;
    i2c_new_master_bus(&i2c_config, &bus_handle);

    //Register the device
    i2c_device_config_t dev_config = DEFAULT_I2C_SENSOR_CONFIG;
    i2c_master_bus_add_device(bus_handle, &dev_config, &sensor.handle.platform.handle);

#if CONFIG_OUCHAT_DEBUG_CAM

    i2c_master_bus_config_t i2c_rpi_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = (gpio_num_t)CONFIG_OUCHAT_DEBUG_CAM_SDA,
            .scl_io_num = (gpio_num_t)CONFIG_OUCHAT_DEBUG_CAM_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags{.enable_internal_pullup = true}
    };

    i2c_master_bus_handle_t rpi_bus_handle;
    i2c_new_master_bus(&i2c_rpi_config, &rpi_bus_handle);

    //Define the rpi i²c configuration
    i2c_device_config_t rpi_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0X13,
            .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t rpi_handle;

    //Register the device
    i2c_master_bus_add_device(rpi_bus_handle, &rpi_config, &rpi_handle);

#endif

    //First start
    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {

        init_leds();

        //Power on sensor and init
        sensor.handle.platform.address = VL53L8CX_DEFAULT_I2C_ADDRESS;
        sensor.handle.platform.bus_config = DEFAULT_I2C_BUS_CONFIG;
        sensor.handle.platform.reset_gpio = OUCHAT_SENSOR_DEFAULT_RST;

        gpio_set_direction(OUCHAT_SENSOR_DEFAULT_RST, GPIO_MODE_OUTPUT);
        gpio_set_level(OUCHAT_SENSOR_DEFAULT_RST, 1);

        //Set the default 15Hz, 8x8 continuous config
        sensor.config = DEFAULT_VL53L8CX_CONFIG;

        esp_err_t status = sensor_init(&sensor);

        while (status != ESP_OK){
            set_color({ .red = 50, .blue = 26});
            ESP_LOGE(LOG_TAG, "Sensor failed, restarting...");

            //Reset the sensor
            Reset_Sensor(&sensor.handle.platform);
            status = sensor_init(&sensor);

            WaitMs(&sensor.handle.platform, 1000);
        }

        //Set the LED color
        set_color({.red = 50, .green = 21});

        //Calibrate the sensor
        vl53l8cx_start_ranging(&sensor.handle);

        ESP_LOGI(LOG_TAG, "Starting sensor calibration");
        calibrate_sensor(&sensor);
        ESP_LOGI(LOG_TAG, "Sensor calibrated on %d points !", sensor.calibration.inliers);

        set_color({.red = 0, .green = 0, .blue = 0});

        //Init motion detection based on the previous calibration
        init_motion_indicator(&sensor);

        //Reset the trigger saved by the bistable 555 circuit
        reset_sensor_trigger(OUCHAT_SENSOR_DEFAULT_RST_TRIGGER);

        //Stop the ranging to set the sensor in "sleep"
        sensor_update_config(&sensor, DEFAULT_VL53L8CX_LP_CONFIG);
        sensor_init_thresholds(&sensor);

        //Clear the queue to allow Autonomous mode
        VL53L8CX_ResultsData results;
        uint8_t ready = 0;

        while (!ready) {
            vl53l8cx_check_data_ready(&sensor.handle, &ready);
        }

        vl53l8cx_get_ranging_data(&sensor.handle, &results);

        ESP_LOGI(LOG_TAG, "Entering deep sleep");

        //Add interrupt pin
        esp_deep_sleep_disable_rom_logging();
        esp_sleep_enable_ext0_wakeup(OUCHAT_SENSOR_DEFAULT_INT, 0);

        gpio_deep_sleep_hold_en();

        //Start the deep sleep
        esp_deep_sleep_start();
    }

    //Update and restart the sensor
    sensor_update_config(&sensor, DEFAULT_VL53L8CX_CONFIG);

    //Get the raw data
    VL53L8CX_ResultsData results;
    uint8_t status, ready = 0;

    process_init();
    uint16_t empty_frames = 0;
    bool side_task = false;

#if CONFIG_OUCHAT_DEBUG_CAM

    //Start the record and get the datetime code in order to link the logs
    uint8_t trigger[] = "Start";
    uint8_t timecode[15] = {0};

    //The timecode format : YYYYmmddHHMMSS
    i2c_master_transmit_receive(rpi_handle, trigger, sizeof trigger, timecode, 15, -1);

    init_tcp_logger(timecode);

#elif CONFIG_OUCHAT_DEBUG_LOGGER

    init_tcp_logger();

#endif

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
            std::string log;
            log.resize(OUCHAT_LOG_SIZE);

            mbedtls_base64_encode(reinterpret_cast<unsigned char*>(log.data()), log.size(), &length, reinterpret_cast<const unsigned char*>(results.distance_mm),sizeof (results.distance_mm));
            tcp_log(log);

#endif

            if (empty_frames != 0 || status) {
                empty_frames = status == 0 ? 0 : empty_frames + status;
            }

        }else if(!side_task){

            //Start the side task without interrupting the reads
            TaskHandle_t xHandle = nullptr;
            xTaskCreatePinnedToCore(side_tasks, "ouchat_side_tasks", 4096, nullptr, 3, &xHandle, 1);
            configASSERT(xHandle);

            side_task = true;
        }
    }

#if CONFIG_OUCHAT_DEBUG_CAM

    //Stop the record
    uint8_t stop[] = "Stop";
    i2c_master_transmit(rpi_handle, stop, sizeof(stop), -1);

#endif

#if CONFIG_OUCHAT_DEBUG_LOGGER

    //Stop the logger before sleep
    stop_tcp_logger();

#endif

    //Wait if the esp is requesting the api
    if (xEventGroupGetBits(api_flags) & REQUEST_FLAG){
        xEventGroupWaitBits(api_flags, REQUEST_DONE_FLAG, pdTRUE,pdTRUE,portMAX_DELAY);
    }

    //Reset the trigger saved by the bistable 555 circuit
    reset_sensor_trigger(OUCHAT_SENSOR_DEFAULT_RST_TRIGGER);

    //Add interrupt pin
    esp_deep_sleep_disable_rom_logging();
    esp_sleep_enable_ext0_wakeup(OUCHAT_SENSOR_DEFAULT_INT, 0);

    //Stop the ranging to set the sensor in "sleep"
    status = sensor_update_config(&sensor, DEFAULT_VL53L8CX_LP_CONFIG);
    status |= sensor_init_thresholds(&sensor);

    while (status != VL53L8CX_STATUS_OK)
    {
        set_color({.red = 50, .blue = 26});
        ESP_LOGE(LOG_TAG, "Sensor failed, restarting...");

        //Reset the sensor
        Reset_Sensor(&sensor.handle.platform);
        sensor.config = DEFAULT_VL53L8CX_LP_CONFIG;

        //Restart and re-init it
        status = sensor_init(&sensor);
        status |= vl53l8cx_start_ranging(&sensor.handle);

        //Restart motion detection and thresholds
        init_motion_indicator(&sensor);
        status |= sensor_init_thresholds(&sensor);

        WaitMs(&sensor.handle.platform, 1000);
    }

    //Make shure the sensor is powerd
    gpio_set_direction(OUCHAT_SENSOR_DEFAULT_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(OUCHAT_SENSOR_DEFAULT_RST, 1);

    gpio_deep_sleep_hold_en();

    //Clear the queue to allow Autonomous mode
    ready = 0;

    while (!ready) {
        vl53l8cx_check_data_ready(&sensor.handle, &ready);
    }

    vl53l8cx_get_ranging_data(&sensor.handle, &results);

    ESP_LOGI(LOG_TAG, "Entering deep sleep");

    //Start the deep sleep
    esp_deep_sleep_start();
}