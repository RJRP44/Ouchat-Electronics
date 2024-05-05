//
// Created by Romain on 28/01/2024.
//

#ifndef OUCHAT_ELECTRONICS_SENSOR_H
#define OUCHAT_ELECTRONICS_SENSOR_H

#include <esp_err.h>
#include <esp_log.h>
#include <vl53l8cx_api.h>
#include <driver/i2c_master.h>
#include <types.h>
#include <utils.h>

#define SENSOR_LOG_TAG "sensor"

#define BACKGROUND_THRESHOLD 150

#define I2C_DEFAULT_TIMEOUT 0x0000001FU
#define I2C_DEFAULT_CLK_SPEED 1000000

#define OUCHAT_SENSOR_DEFAULT_SDA GPIO_NUM_1
#define OUCHAT_SENSOR_DEFAULT_SCL GPIO_NUM_2
#define OUCHAT_SENSOR_DEFAULT_INT GPIO_NUM_3
#define OUCHAT_SENSOR_DEFAULT_RST_TRIGGER GPIO_NUM_4
#define OUCHAT_SENSOR_DEFAULT_RST GPIO_NUM_5

//Default i2c, and sensor configs
#define DEFAULT_I2C_BUS_CONFIG {                            \
        .i2c_port = I2C_NUM_1,                              \
        .sda_io_num = OUCHAT_SENSOR_DEFAULT_SDA,                           \
        .scl_io_num = OUCHAT_SENSOR_DEFAULT_SCL,                           \
        .clk_source = I2C_CLK_SRC_DEFAULT,                  \
        .glitch_ignore_cnt = 7,                             \
        .intr_priority = 0,                                 \
        .trans_queue_depth = 0,                             \
        .flags{.enable_internal_pullup = true}              \
}

#define DEFAULT_I2C_SENSOR_CONFIG {                         \
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,              \
        .device_address = VL53L8CX_DEFAULT_I2C_ADDRESS >> 1,\
        .scl_speed_hz = VL53L8CX_MAX_CLK_SPEED,             \
};

#define DEFAULT_VL53L8CX_CONFIG {                           \
        .resolution = VL53L8CX_RESOLUTION_8X8,              \
        .frequency = 15,                                    \
        .mode = VL53L8CX_RANGING_MODE_CONTINUOUS,           \
}

#define DEFAULT_VL53L8CX_LP_CONFIG {                        \
        .resolution = VL53L8CX_RESOLUTION_8X8,              \
        .frequency = 5,                                     \
        .mode = VL53L8CX_RANGING_MODE_AUTONOMOUS,           \
        .integration_time = 5,                             \
}

esp_err_t sensor_init(sensor_t *sensor);
esp_err_t init_motion_indicator(sensor_t *sensor);
esp_err_t sensor_update_config(sensor_t *sensor, sensor_config_t config);
esp_err_t sensor_init_thresholds(sensor_t *sensor);
esp_err_t reset_sensor_trigger(gpio_num_t gpio);

#endif //OUCHAT_ELECTRONICS_SENSOR_H
