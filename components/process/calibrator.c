//
// Created by Romain on 28/01/2024.
//

#include "include/calibrator.h"
#include <vl53l8cx_api.h>
#include <ouchat_utils.h>
#include <ouchat_types.h>
#include <memory.h>

static uint8_t pixel_angles(p_coord_t coordinates, p_angles_t *angles) {

    //The angle is proportional to the position on the sensor
    angles->x = (coordinates.x - 3.5)           //Get the coordinates with a (0;0) at the center
                * (VL53L8CX_SENSOR_FOV / 7);    //The more the point is far from the center the more it is angled

    angles->y = (coordinates.y - 3.5) * (VL53L8CX_SENSOR_FOV / 7);

    return 0;
}

static uint8_t pixel_coordinates(coord_t *coordinates, double measure, p_coord_t coords) {
    //Get the angle of the measure
    p_angles_t angles;

    pixel_angles(coords, &angles);

    //Get the coordinates
    coordinates->x = measure * sin(angles.x * PI / 180);
    coordinates->y = measure * sin(angles.y * PI / 180);
    coordinates->z = measure;

    return 0;
}

marker one = 1;

void determine_floor(int pool, int need, marker chosen, int at, double calibrating_dataset[8][8], plane_t *floor_plane,
                     uint8_t *inliers) {
    if (pool < need + at) return; /* not enough bits left */

    if (!need) {

        //Get the plane for 3 points : A, B, C
        coord_t points[3];
        p_coord_t pixels[3];

        uint8_t point_counter = 0;

        for (at = 0; at < pool; at++) {
            if (chosen & (one << at)) {
                uint8_t x = ABSCISSA_FROM_1D(at);
                uint8_t y = ORDINATE_FROM_1D(at);
                pixels[point_counter] = (p_coord_t) {x, y};
                pixel_coordinates(&points[point_counter], calibrating_dataset[x][y], pixels[point_counter]);

                point_counter++;
            }
        }

        //Make sure that the 3 points aren't collinear
        double det = points[0].x * points[1].y * points[2].z + points[1].x * points[2].y * points[0].z +
                     points[2].x * points[0].y * points[1].z - points[0].z * points[1].y * points[2].x -
                     points[1].z * points[2].y * points[0].x - points[2].z * points[0].y * points[1].x;

        if (fabs(det) < 0.0000001) {
            return;
        }

        //Coefficient of our plane
        plane_t plane;

        //coordinate vector from AB∧AC
        plane.a = (points[1].y - points[0].y) * (points[2].z - points[0].z) -
                  (points[1].z - points[0].z) * (points[2].y - points[0].y);
        plane.b = (points[1].z - points[0].z) * (points[2].x - points[0].x) -
                  (points[1].x - points[0].x) * (points[2].z - points[0].z);
        plane.c = (points[1].x - points[0].x) * (points[2].y - points[0].y) -
                  (points[1].y - points[0].y) * (points[2].x - points[0].x);

        //Calculate the value of d
        plane.d = plane.a * points[0].x + plane.b * points[0].y + plane.c * points[0].z;

        uint8_t plane_inliers = 0;

        //For each other points calculate the distance from the plane
        for (X_Y_FOR_LOOP) {

            for (int j = 0; j < 3; ++j) {
                if (pixels[j].x == x && pixels[j].y == y) {
                    goto end;
                }
            }


            coord_t point;
            pixel_coordinates(&point, calibrating_dataset[x][y], (p_coord_t) {x, y});
            double distance = fabs(plane.a * point.x + plane.b * point.y + plane.c * point.z - plane.d) /
                              sqrt(pow(plane.a, 2) + pow(plane.b, 2) + pow(plane.c, 2));

            if (distance < INLIER_THRESHOLD_VALUE) {
                plane_inliers++;
            }

            end:
            continue;
        }

        if (plane_inliers > *inliers) {
            *floor_plane = plane;
            *inliers = plane_inliers;
        }

        return;
    }
    //if we choose the current item, "or" (|) the bit to mark it so.
    determine_floor(pool, need - 1, chosen | (one << at), at + 1, calibrating_dataset, floor_plane, inliers);
    determine_floor(pool, need, chosen, at + 1, calibrating_dataset, floor_plane, inliers);  /* or don't choose it, go to next */
}

esp_err_t calibrate_sensor(sensor_t *sensor){
    uint16_t raw_calibration_data[OUCHAT_CALIBRATION_DATASET_SIZE][8][8];

    for (int i = 0; i < OUCHAT_CALIBRATION_DATASET_SIZE; ++i) {

        //Check if data is available
        uint8_t is_ready = 0;
        vl53l8cx_check_data_ready(&sensor->handle, &is_ready);

        //Wait for the sensor to start ranging
        while (!is_ready) {
            WaitMs(&(sensor->handle.platform), 5);
            vl53l8cx_check_data_ready(&sensor->handle, &is_ready);
        }

        //Get the raw data
        VL53L8CX_ResultsData results;
        vl53l8cx_get_ranging_data(&sensor->handle, &results);

        memcpy(raw_calibration_data[i], results.distance_mm, sizeof raw_calibration_data[i]);
    }

    //Do an average to avoid noise
    uint32_t sum[8][8] = {0};
    double calibration_data[8][8] = {0};

    for (int i = 0; i < OUCHAT_CALIBRATION_DATASET_SIZE; ++i) {
        for (X_Y_FOR_LOOP) {
            sum[x][y] += raw_calibration_data[i][x][y];
        }
    }

    for (X_Y_FOR_LOOP) {
        calibration_data[x][y] = (double) sum[x][y] / OUCHAT_CALIBRATION_DATASET_SIZE;
        sensor->calibration.background[x][y] = (uint16_t) round(calibration_data[x][y]);

        if (sensor->calibration.furthest_point > (uint16_t) round(calibration_data[x][y])){
            continue;
        }

        sensor->calibration.furthest_point = (uint16_t) round(calibration_data[x][y]);
    }

    sensor->calibration.inliers = 0;
    determine_floor(64, 3, 0, 0, calibration_data, &sensor->calibration.floor, &sensor->calibration.inliers);

    //Calculate the angles of the floor in oder to compensate them after
    double norm = sqrt(pow(sensor->calibration.floor.a, 2) + pow(sensor->calibration.floor.b, 2) + pow(sensor->calibration.floor.c, 2));
    sensor->calibration.angles.x = acos(sensor->calibration.floor.a / norm);
    sensor->calibration.angles.y = acos(sensor->calibration.floor.b / norm);
    sensor->calibration.angles.z = acos(sensor->calibration.floor.c / norm);

    //Calculate the average floor distance
    coord_t floor_data[8][8];
    double height_sum = 0;
    correct_sensor_data(sensor, sensor->calibration.background, floor_data);

    for (X_Y_FOR_LOOP){
        height_sum += floor_data[x][y].z;
    }

    sensor->calibration.floor_distance = height_sum / 64;

    return ESP_OK;
}

esp_err_t correct_sensor_data(sensor_t *sensor, uint16_t raw_data[8][8], coord_t output[8][8]){

    //For each point, convert the sensor's measure to a Galilean reference frame coordinates
    for (int x = 0, y = 0; x < 8 && y < 8; y = (y + 1) % 8, x += y ? 0 : 1) {

        coord_t coordinates_sensor, z_rotated_coordinates;
        uint16_t measure = raw_data[x][y];

        pixel_coordinates(&coordinates_sensor, measure, (p_coord_t) {x, y});

        //Rotate around the z-axis
        z_rotated_coordinates.x = coordinates_sensor.x;

        z_rotated_coordinates.y = cos(PI - sensor->calibration.angles.z) * coordinates_sensor.y +
                                  sin(PI - sensor->calibration.angles.z) * coordinates_sensor.z;

        z_rotated_coordinates.z = -sin(PI - sensor->calibration.angles.z) * coordinates_sensor.y +
                                  cos(PI - sensor->calibration.angles.z) * coordinates_sensor.z;

        //Rotate around the x-axis
        output[x][y].x = cos(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.x +
                         sin(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.z;

        output[x][y].y = z_rotated_coordinates.y;

        output[x][y].z = -sin(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.x +
                         cos(PI / 2 - sensor->calibration.angles.x) * z_rotated_coordinates.z;

    }
    return ESP_OK;

}