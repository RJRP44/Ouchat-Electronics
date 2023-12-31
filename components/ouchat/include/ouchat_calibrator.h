//
// Created by Romain on 26/12/2023.
//

#ifndef OUCHAT_ELECTRONICS_OUCHAT_CALIBRATOR_H
#define OUCHAT_ELECTRONICS_OUCHAT_CALIBRATOR_H

#include "stdint.h"
#include "ouchat_units.h"
#include "vl53l8cx_api.h"

#define PI 3.14159265359

#define VL53L8CX_SENSOR_FOV 45.0F

#define X_Y_FOR_LOOP int x = 0, y = 0; x < 8 && y < 8; y = (y + 1) % 8, x += y ? 0 : 1
#define INLIER_THRESHOLD_VALUE 15
#define OUCHAT_CALIBRATION_DATASET_SIZE 40

typedef struct {
    plane_angles_t angles;
    plane_t floor;
    uint8_t inliers;
    uint16_t background[8][8];
    double floor_distance;
} ouchat_calibration_config;

typedef unsigned long long marker;

uint8_t pixel_angles(pixel_coordinates_t coordinates, pixel_angles_t *angles);
uint8_t ouchat_calibrate_sensor(ouchat_calibration_config *calibration_configuration, VL53L8CX_Configuration *ouchat_sensor_configuration);
uint8_t ouchat_correct_data(ouchat_calibration_config config, uint16_t raw_data[8][8], coordinates_t output[8][8]);

#endif //OUCHAT_ELECTRONICS_OUCHAT_CALIBRATOR_H
