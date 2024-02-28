//
// Created by Romain on 29/12/2023.
//

#ifndef OUCHAT_DATA_PREPROCESSING_OUCHAT_UNITS_H
#define OUCHAT_DATA_PREPROCESSING_OUCHAT_UNITS_H

#include <stdint.h>
#include <vl53l8cx_api.h>
#include <vl53l8cx_plugin_detection_thresholds.h>
#include <vl53l8cx_plugin_motion_indicator.h>

typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} color_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} p_coord_t;

typedef struct {
    double x, y, z;
} coord_t;

typedef struct {
    coord_t coord;
    int16_t cluster_id;
} point_t;

typedef struct {
    uint16_t age;
    coord_t entry_coord;
    coord_t exit_coord;
    double average_deviation;
    double average_height;
    coord_t maximum;
    coord_t minimum;
} tracker_t;

typedef struct {
    int16_t id;
    uint8_t size;
    tracker_t tracker;
    coord_t coord;
} cluster_t;

typedef struct {
    point_t data[8][8];
    int16_t clusters_count;
    cluster_t *clusters;
    int8_t background_count;
} frame_t;

typedef struct {
    double a, b;
} p_angles_t;

typedef struct {
    double a, b, c, d;
} plane_t;

typedef struct {
    double x, y, z;
} plane_angles_t;

typedef struct {
    plane_angles_t angles;
    plane_t floor;
    uint8_t inliers;
    uint16_t background[8][8];
    double outliers[8][8];
    uint16_t furthest_point;
    double floor_distance;
} calibration_config_t;

typedef struct{
    uint8_t resolution;
    uint8_t frequency;
    uint8_t mode;
    uint32_t integration_time;
} sensor_config_t;

typedef struct {
    VL53L8CX_Configuration handle;
    sensor_config_t config;
    calibration_config_t calibration;
    VL53L8CX_DetectionThresholds thresholds[VL53L8CX_NB_THRESHOLDS];
    VL53L8CX_Motion_Configuration motion_config;
} sensor_t;

#endif //OUCHAT_DATA_PREPROCESSING_OUCHAT_UNITS_H
