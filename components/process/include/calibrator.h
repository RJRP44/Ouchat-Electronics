//
// Created by Romain on 28/01/2024.
//

#ifndef OUCHAT_ELECTRONICS_CALIBRATOR_H
#define OUCHAT_ELECTRONICS_CALIBRATOR_H

#include <unistd.h>
#include <types.h>
#include <sensor.h>
#include <assert.h>
#include <math.h>
#include <esp_err.h>

#define PI 3.14159265359

#define VL53L8CX_SENSOR_FOV 45.0F

#define INLIER_THRESHOLD_VALUE 15
#define OUCHAT_CALIBRATION_DATASET_SIZE 40

typedef unsigned long long marker;

esp_err_t calibrate_sensor(sensor_t *sensor);

esp_err_t correct_sensor_data(sensor_t *sensor, uint16_t raw_data[8][8], coord_t output[8][8]);

#endif //OUCHAT_ELECTRONICS_CALIBRATOR_H
