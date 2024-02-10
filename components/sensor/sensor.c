//
// Created by Romain on 28/01/2024.
//

#include "include/sensor.h"
#include "vl53l8cx_plugin_motion_indicator.h"
#include <ouchat_types.h>

esp_err_t init_i2c(i2c_port_t port, i2c_config_t config) {

    //Initialize the i2c bus
    i2c_param_config(port, &config);
    i2c_set_timeout(port, I2C_DEFAULT_TIMEOUT);

    return i2c_driver_install(port, config.mode, 0, 0, 0);
}

esp_err_t sensor_init(sensor_t *sensor) {

    //Send the configuration to the sensor
    uint8_t status, alive = 0;

    status = vl53l8cx_is_alive(&sensor->handle, &alive);
    if (!alive || status) {
        ESP_LOGE(SENSOR_LOG_TAG, "VL53L8CX not detected at %X", sensor->handle.platform.address);
        return ESP_FAIL;
    }

    status = vl53l8cx_init(&sensor->handle);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "VL53L8CX ULD Loading failed");
        return ESP_FAIL;
    }

    status = vl53l8cx_set_resolution(&sensor->handle, sensor->config.resolution);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "VL53L8CX set resolution to %d failed", sensor->config.resolution);
        return ESP_FAIL;
    }

    status = vl53l8cx_set_ranging_frequency_hz(&sensor->handle, sensor->config.frequency);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "VL53L8CX set frequency to %dHz failed", sensor->config.frequency);
        return ESP_FAIL;
    }

    status = vl53l8cx_set_ranging_mode(&sensor->handle, sensor->config.mode);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "VL53L8CX set ranging mode failed");
        return ESP_FAIL;
    }

    if (sensor->config.mode != VL53L8CX_RANGING_MODE_AUTONOMOUS) {
        return ESP_OK;
    }

    //Set the integration time only if the sensor is in autonomous mode
    status = vl53l8cx_set_integration_time_ms(&sensor->handle, sensor->config.integration_time);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "VL53L8CX set integration time failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t init_motion_indicator(sensor_t *sensor) {

    uint8_t status = vl53l8cx_motion_indicator_init(&sensor->handle, &sensor->motion_config, VL53L8CX_RESOLUTION_8X8);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "Motion indicator init failed with status : %u", status);
        return ESP_FAIL;
    }

    //Calculate the trigger distances
    uint16_t min, max;
    min = (uint16_t) sensor->calibration.floor_distance - AVERAGE_CAT_HEIGHT;

    //The minimum distance for indicator is 400mm
    if (min < 400){
        min = 400;
        ESP_LOGW(SENSOR_LOG_TAG, "The sensor seems too close to the floor, which can lead to malfunctions");
    }

    max = sensor->calibration.furthest_point;

    //The distance between the min and the max mustn't exceed 1500mm
    if (max - min > 1500){
        max = min + 1500;
    }

    ESP_LOGI(SENSOR_LOG_TAG, "Motion indicator(%d,%d)",min,max);

    status = vl53l8cx_motion_indicator_set_distance_motion(&sensor->handle, &sensor->motion_config, min, max);
    if (status) {
        ESP_LOGE(SENSOR_LOG_TAG, "Motion indicator set distance motion failed with status : %u", status);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t sensor_update_config(sensor_t *sensor, sensor_config_t config){
    vl53l8cx_stop_ranging(&sensor->handle);
    
    //Apply the new configuration
    if (sensor->config.resolution != config.resolution){
        vl53l8cx_set_resolution(&sensor->handle, config.resolution);
    }

    if (sensor->config.frequency != config.frequency){
        vl53l8cx_set_ranging_frequency_hz(&sensor->handle, config.frequency);
    }

    if (sensor->config.mode != config.mode){
        vl53l8cx_set_ranging_mode(&sensor->handle, config.mode);
    }

    if (config.mode != VL53L8CX_RANGING_MODE_AUTONOMOUS) {
        goto end;
    }

    if (sensor->config.integration_time != config.integration_time){
        vl53l8cx_set_integration_time_ms(&sensor->handle, config.integration_time);
    }

    end:

    //Copy the new configuration
    memcpy(&sensor->config, &config, sizeof(config));

    vl53l8cx_start_ranging(&sensor->handle);

    return ESP_OK;
}

esp_err_t sensor_init_thresholds(sensor_t *sensor){

    //Stop ranging
    vl53l8cx_stop_ranging(&sensor->handle);

    //Clear old thresholds
    memset(&sensor->thresholds, 0, sizeof(sensor->thresholds));

    //Add thresholds for all zones
    for(int i = 0; i < 64; i++){
        sensor->thresholds[i].zone_num = i;
        sensor->thresholds[i].measurement = VL53L8CX_MOTION_INDICATOR;
        sensor->thresholds[i].type = VL53L8CX_GREATER_THAN_MAX_CHECKER;
        sensor->thresholds[i].mathematic_operation = VL53L8CX_OPERATION_NONE;

        sensor->thresholds[i].param_low_thresh = 44;
        sensor->thresholds[i].param_high_thresh = 44;
    }

    //Define the last threshold
    sensor->thresholds[63].zone_num = VL53L8CX_LAST_THRESHOLD | sensor->thresholds[63].zone_num;

    //Enable detection thresholds
    vl53l8cx_set_detection_thresholds(&sensor->handle, sensor->thresholds);
    vl53l8cx_set_detection_thresholds_enable(&sensor->handle, 1);
    vl53l8cx_set_detection_thresholds_auto_stop(&sensor->handle, 1);

    vl53l8cx_start_ranging(&sensor->handle);

    return ESP_OK;
}