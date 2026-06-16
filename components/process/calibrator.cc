//
// Created by Romain on 28/01/2024.
//

#include "include/calibrator.h"
#include <vl53l8cx_api.h>
#include <utils.h>
#include <types.h>
#include "esp_random.h"

static uint8_t pixel_angles(p_coord_t coordinates, p_angles_t *angles) {

    //The angle is proportional to the position on the sensor
    angles->a = (coordinates.x - 3.5)           //Get the coordinates with a (0;0) at the center
                * (VL53L8CX_SENSOR_FOV / 7);    //The more the point is far from the center the more it is angled

    angles->b = (coordinates.y - 3.5) * (VL53L8CX_SENSOR_FOV / 7);

    return 0;
}

static uint8_t pixel_coordinates(coord_t *coordinates, double measure, p_coord_t coords) {
    //Get the angle of the measure
    p_angles_t angles;

    pixel_angles(coords, &angles);

    //Get the coordinates
    coordinates->x = measure * sin(angles.a * PI / 180);
    coordinates->y = measure * sin(angles.b * PI / 180);
    coordinates->z = measure;

    return 0;
}

static double distance_plane_to_point(const plane_t& plane, const coord_t& point)
{
    return fabs(plane.a * point.x + plane.b * point.y + plane.c * point.z - plane.d) /
                              sqrt(pow(plane.a, 2) + pow(plane.b, 2) + pow(plane.c, 2));
}

void fit(coord_t dataset[64], plane_t *floor_plane, uint8_t *inliers)
{
    *inliers = 0;
    double best_error = INFINITY;
    for (int i = 0; i < CALIBRATION_RANSAC_ITERATIONS; i++)
    {

        //We want 3 random values from 0 to 64
        uint32_t seed = esp_random();

        uint8_t a = seed & 63;
        uint8_t b = seed >> 7 & 63;
        uint8_t c = seed >> 14 & 63;

        if (a == b || a == c || b == c)
        {
            i--;
            continue;
        }

        coord_t points[3];

        points[0] = dataset[a];
        points[1] = dataset[b];
        points[2] = dataset[c];

        //It is not useful to create a plane with a null point
        if (points[0].empty())
        {
            i--;
            continue;
        }

        if (points[1].empty())
        {
            i--;
            continue;
        }

        if (points[2].empty())
        {
            i--;
            continue;
        }

        //Make sure that the 3 points aren't collinear
        double det = points[0].x * points[1].y * points[2].z + points[1].x * points[2].y * points[0].z +
                     points[2].x * points[0].y * points[1].z - points[0].z * points[1].y * points[2].x -
                     points[1].z * points[2].y * points[0].x - points[2].z * points[0].y * points[1].x;

        if (fabs(det) < 0.0000001) {
            continue;
        }

        //Coefficient of our plane
        plane_t plane;

        //Coordinate vector from AB∧AC
        plane.a = (points[1].y - points[0].y) * (points[2].z - points[0].z) -
                  (points[1].z - points[0].z) * (points[2].y - points[0].y);
        plane.b = (points[1].z - points[0].z) * (points[2].x - points[0].x) -
                  (points[1].x - points[0].x) * (points[2].z - points[0].z);
        plane.c = (points[1].x - points[0].x) * (points[2].y - points[0].y) -
                  (points[1].y - points[0].y) * (points[2].x - points[0].x);

        //Calculate the value of d
        plane.d = plane.a * points[0].x + plane.b * points[0].y + plane.c * points[0].z;

        double error = 0;
        uint8_t plane_inliers = 0;

        //For each other points calculate the distance from the plane in order to get the error
        for (int j = 0; j < 64; j++)
        {
            //The error is 0 for those 3 points
            if (j == a || j == b || j == c)
            {
                plane_inliers++;
                continue;
            }

            //Skip if null
            if (dataset[j].empty())
                continue;

            double distance = distance_plane_to_point(plane,dataset[j]);


            if (distance < INLIER_THRESHOLD_VALUE) {
                plane_inliers++;
            }
            error += pow(plane_inliers, 2);
        }

        //If the new plane has less error it is considered better
        if (plane_inliers > *inliers || (error < best_error && plane_inliers == *inliers))
        {
            best_error = error;
            *floor_plane = plane;
            *inliers = plane_inliers;
        }
    }
}

esp_err_t calibrate_sensor(sensor_t *sensor){
    measurement_t raw_calibration_data[OUCHAT_CALIBRATION_DATASET_SIZE][8][8];

    for (auto & i : raw_calibration_data) {

        //Check if data is available
        uint8_t is_ready = 0;
        vl53l8cx_check_data_ready(&sensor->handle, &is_ready);

        //Wait for the sensor to start ranging
        while (!is_ready) {
            vTaskDelay(5 / portTICK_PERIOD_MS);
            vl53l8cx_check_data_ready(&sensor->handle, &is_ready);
        }

        //Get the raw data
        VL53L8CX_ResultsData results;
        vl53l8cx_get_ranging_data(&sensor->handle, &results);

        for (X_Y_FOR_LOOP) {
            i[x][y].distance = results.distance_mm[x * 8 + y];
            i[x][y].status = results.target_status[x * 8 + y];
        }
    }

    //Do an average to avoid noise
    uint32_t sum[8][8] = {};

    //For each pixel count the number of valid measurements
    uint8_t valid_measurements[8][8] = {};

    for (const auto & i : raw_calibration_data) {
        for (X_Y_FOR_LOOP) {

            //According to uld's guide, um2884 5, 9 and 10 status code doesn't impact measurements
            if (i[x][y].status != 5 && i[x][y].status != 9 && i[x][y].status != 10)
                continue;

            sum[x][y] += i[x][y].distance;
            valid_measurements[x][y]++;
        }
    }

    coord_t dataset[64] = {};

    for (X_Y_FOR_LOOP) {

        //If there is not a single correct measurement we shouldn't take this into the calibration
        if (valid_measurements[x][y] == 0)
        {
            continue;
        }

        double distance = static_cast<double>(sum[x][y]) / valid_measurements[x][y];
        sensor->calibration.background[x][y] = static_cast<uint16_t>(round(distance));

        pixel_coordinates(&dataset[x * 8 + y], distance, {.x = x,.y = y});

        if (sensor->calibration.furthest_point > static_cast<uint16_t>(round(distance))){
            continue;
        }

        sensor->calibration.furthest_point = static_cast<uint16_t>(round(distance));
    }

    sensor->calibration.inliers = 0;

    fit(dataset, &sensor->calibration.floor, &sensor->calibration.inliers);

    //Calculate the angles of the floor in oder to compensate them after
    double norm = sqrt(pow(sensor->calibration.floor.a, 2) + pow(sensor->calibration.floor.b, 2) + pow(sensor->calibration.floor.c, 2));
    sensor->calibration.angles.x = acos(fabs(sensor->calibration.floor.a) / norm);
    sensor->calibration.angles.y = acos(fabs(sensor->calibration.floor.b) / norm);
    sensor->calibration.angles.z = acos(fabs(sensor->calibration.floor.c) / norm);

    sensor->calibration.floor_distance = fabs(sensor->calibration.floor.d) / norm;

    ESP_LOGI(SENSOR_LOG_TAG, "Distance to the floor : %.1f",sensor->calibration.floor_distance);

    //Calculate the average floor distance
    coord_t floor_data[8][8];
    correct_sensor_data(sensor, sensor->calibration.background, floor_data);

    for (X_Y_FOR_LOOP){
        bool is_outlier = fabs(floor_data[x][y].z - sensor->calibration.floor_distance) > BACKGROUND_THRESHOLD;
        sensor->calibration.outliers[x][y] = is_outlier ? sensor->calibration.floor_distance : -1;
    }

    return ESP_OK;
}

esp_err_t correct_sensor_data(sensor_t *sensor, uint16_t raw_data[8][8], coord_t output[8][8]){

    //For each point, convert the sensor's measure to a Galilean reference frame coordinates
    for (X_Y_FOR_LOOP) {

        coord_t coordinates_sensor, z_rotated_coordinates;
        uint16_t measure = raw_data[x][y];

        pixel_coordinates(&coordinates_sensor, measure, {x, y});

        //Rotate around the z-axis
        z_rotated_coordinates.x = coordinates_sensor.x;

        z_rotated_coordinates.y = cos(sensor->calibration.angles.z) * coordinates_sensor.y +
                                  sin(sensor->calibration.angles.z) * coordinates_sensor.z;

        z_rotated_coordinates.z = -sin(sensor->calibration.angles.z) * coordinates_sensor.y +
                                  cos(sensor->calibration.angles.z) * coordinates_sensor.z;

        //Rotate around the x-axis
        output[x][y].x = cos(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.x +
                         sin(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.z;

        output[x][y].y = z_rotated_coordinates.y;

        output[x][y].z = -sin(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.x +
                         cos(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.z;

    }
    return ESP_OK;

}