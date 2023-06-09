//
// Created by RJRP on 09/06/2023.
//

#include "ouchat_sensor.h"
#include "vl53l5cx_plugin_motion_indicator.h"
#include "vl53l5cx_plugin_detection_thresholds.h"

uint8_t ouchat_init_i2c(i2c_port_t port, i2c_config_t config){
    i2c_param_config(port, &config);
    i2c_set_timeout(port, I2C_DEFAULT_TIMEOUT);

    return i2c_driver_install(port, config.mode, 0, 0, 0);
}

uint8_t ouchat_init_sensor(ouchat_sensor_config config){

    //Power on sensor and init
    uint8_t status, isAlive;

    status = vl53l5cx_is_alive(config.sensor_config, &isAlive);
    if (!isAlive || status) {
        printf("VL53L5CX not detected at requested address\n");
        return 1;
    }
    printf("VL53L5CX detected at requested address : %X\n", config.sensor_config->platform.address);

    status = vl53l5cx_init(config.sensor_config);
    if (status) {
        printf("VL53L5CX ULD Loading failed\n");
        return 1;
    }
    printf("VL53L5CX ULD ready ! (Version : %s)\n", VL53L5CX_API_REVISION);

    status = vl53l5cx_set_resolution(config.sensor_config, config.resolution);
    if (status){
        printf("VL53L5CX set resolution failed\n");
        return 1;
    }
    printf("VL53L5CX set resolution to %d\n", config.resolution);

    status = vl53l5cx_set_ranging_frequency_hz(config.sensor_config, config.frequency_hz);
    if (status){
        printf("VL53L5CX set frequency failed\n");
        return 1;
    }
    printf("VL53L5CX set frequency to %d\n", config.frequency_hz);

    status = vl53l5cx_set_ranging_mode(config.sensor_config, config.ranging_mode);
    if (status){
        printf("VL53L5CX set ranging mode failed\n");
        return 1;
    }
    printf("VL53L5CX set ranging mode to %s\n", config.ranging_mode == VL53L5CX_RANGING_MODE_AUTONOMOUS ? "autonomous" : "continuous");

    if (config.ranging_mode != VL53L5CX_RANGING_MODE_AUTONOMOUS){
        return 0;
    }

    //Set the integration time only if the sensor is in autonomous mode
    status = vl53l5cx_set_integration_time_ms(config.sensor_config, config.integration_time);
    if (status){
        printf("VL53L5CX set integration time failed\n");
        return 1;
    }
    printf("VL53L5CX set integration time to %lu\n", config.integration_time);

    return 0;
}

uint8_t ouchat_lp_sensor(ouchat_sensor_config config, ouchat_motion_threshold_config motion_threshold_config){

    VL53L5CX_Motion_Configuration motion_config;
    uint8_t status = 0;

    status = vl53l5cx_motion_indicator_init(config.sensor_config, &motion_config, config.resolution);
    if(status)
    {
        printf("Motion indicator init failed with status : %u\n", status);
        return 1;
    }
    printf("Motion indicator ready !");

    status = vl53l5cx_motion_indicator_set_distance_motion(config.sensor_config, &motion_config, motion_threshold_config.distance_min, motion_threshold_config.distance_max);
    if(status)
    {
        printf("Motion indicator set distance motion failed with status : %u\n", status);
        return 1;
    }
    printf("Motion indicator set distance motion min : ");

    //ouchat_init_sensor(config);

    status = vl53l5cx_set_resolution(config.sensor_config, VL53L5CX_RESOLUTION_8X8);
    status = vl53l5cx_set_ranging_mode(config.sensor_config, VL53L5CX_RANGING_MODE_AUTONOMOUS);
    status = vl53l5cx_set_ranging_frequency_hz(config.sensor_config, 5);
    status = vl53l5cx_set_integration_time_ms(config.sensor_config, 10);

    //Define & setup thresholds
    VL53L5CX_DetectionThresholds motion_thresholds[VL53L5CX_NB_THRESHOLDS];

    memset(motion_thresholds, 0, sizeof(motion_thresholds));

    //Add thresholds for all zones
    for(int i = 0; i < 64; i++){
        motion_thresholds[i].zone_num = i;
        motion_thresholds[i].measurement = VL53L5CX_MOTION_INDICATOR;
        motion_thresholds[i].type = VL53L5CX_GREATER_THAN_MAX_CHECKER;
        motion_thresholds[i].mathematic_operation = VL53L5CX_OPERATION_NONE;

        /* The value 44 is given as example. All motion above 44 will be considered as a movement */
        motion_thresholds[i].param_low_thresh = 44;
        motion_thresholds[i].param_high_thresh = 44;
    }

    //Define the last threshold
    motion_thresholds[63].zone_num = VL53L5CX_LAST_THRESHOLD | motion_thresholds[63].zone_num;

    //Enable detection thresholds
    vl53l5cx_set_detection_thresholds(config.sensor_config, motion_thresholds);
    vl53l5cx_set_detection_thresholds_enable(config.sensor_config, 1);

    return 0;
}