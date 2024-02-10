//
// Created by Romain on 28/01/2024.
//

#ifndef OUCHAT_ELECTRONICS_SENSOR_H
#define OUCHAT_ELECTRONICS_SENSOR_H

#include <esp_err.h>
#include <esp_log.h>
#include <vl53l8cx_api.h>
#include <vl53l8cx_plugin_detection_thresholds.h>
#include <driver/i2c.h>
#include <ouchat_types.h>

#define SENSOR_LOG_TAG "sensor"

#define BACKGROUND_THRESHOLD 150

#define I2C_DEFAULT_TIMEOUT 0x0000001FU
#define I2C_DEFAULT_CLK_SPEED 1000000

#define OUCHAT_SENSOR_DEFAULT_SDA 1
#define OUCHAT_SENSOR_DEFAULT_SCL 2
#define OUCHAT_SENSOR_DEFAULT_INT 3

#define AVERAGE_CAT_HEIGHT 250

//Default i2c, and sensor configs
#define DEFAULT_I2C_CONFIG (i2c_config_t){         \
        .mode = I2C_MODE_MASTER,                   \
        .sda_io_num = OUCHAT_SENSOR_DEFAULT_SDA,   \
        .scl_io_num = OUCHAT_SENSOR_DEFAULT_SCL,   \
        .sda_pullup_en = GPIO_PULLUP_ENABLE,       \
        .scl_pullup_en = GPIO_PULLUP_ENABLE,       \
        .master.clk_speed = I2C_DEFAULT_CLK_SPEED, \
}

#define DEFAULT_VL53L8CX_CONFIG (sensor_config_t) {         \
        .mode = VL53L8CX_RANGING_MODE_CONTINUOUS,           \
        .frequency = 15,                                    \
        .resolution = VL53L8CX_RESOLUTION_8X8,              \
}

#define DEFAULT_VL53L8CX_LP_CONFIG (sensor_config_t) {      \
        .mode = VL53L8CX_RANGING_MODE_AUTONOMOUS,           \
        .frequency = 5,                                     \
        .resolution = VL53L8CX_RESOLUTION_8X8,              \
        .integration_time = 10,                             \
}

esp_err_t init_i2c(i2c_port_t port, i2c_config_t config);
esp_err_t sensor_init(sensor_t *sensor);
esp_err_t init_motion_indicator(sensor_t *sensor);
esp_err_t sensor_update_config(sensor_t *sensor, sensor_config_t config);
esp_err_t sensor_init_thresholds(sensor_t *sensor);

#endif //OUCHAT_ELECTRONICS_SENSOR_H
