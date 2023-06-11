//
// Created by RJRP on 09/06/2023.
//

#include "stdint.h"
#include "platform.h"
#include "vl53l5cx_api.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_SENSOR_H
#define OUCHAT_ELECTRONICS_OUCHAT_SENSOR_H

#define I2C_DEFAULT_TIMEOUT 0x0000001FU
#define I2C_DEFAULT_CLK_SPEED 1000000
#define OUCHAT_SENSOR_DEFAULT_SDA 1
#define OUCHAT_SENSOR_DEFAULT_SCL 2

typedef struct {
    VL53L5CX_Configuration *sensor_config;
    uint8_t resolution;
    uint8_t frequency_hz;
    uint8_t ranging_mode;
    uint32_t integration_time;

} ouchat_sensor_config;

typedef struct {
    uint16_t distance_min;
    uint16_t distance_max;
    int32_t motion_threshold;
} ouchat_motion_threshold_config;

uint8_t ouchat_init_i2c(i2c_port_t port, i2c_config_t config);
uint8_t ouchat_init_sensor(ouchat_sensor_config config);
uint8_t ouchat_lp_sensor(ouchat_sensor_config config, ouchat_motion_threshold_config motion_threshold_config, int16_t *sensor_background);

#endif //OUCHAT_ELECTRONICS_OUCHAT_SENSOR_H
