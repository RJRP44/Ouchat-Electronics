//
// Created by RJRP on 24/06/2022.
//

#ifndef OUCHAT_PROCESSOR_H_
#define OUCHAT_PROCESSOR_H_

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "vl53l5cx_api.h"
#include "ouchat_utils.h"

#define X_FROM_1D(i) (i - (i % 8)) / 8
#define Y_FROM_1D(i) i % 8
#define C_TO_1D(x, y) 8*(x)+(y)

#define OUCHAT_API_VERBOSE 1u

#define TOTAL_SUM 0
#define ABSCISSA_SUM 1
#define ORDINATE_SUM 2

#define PI 3.14159265

typedef struct {
    double_t m;
    double_t p;
} cat_trajectory_t;

/**
 * @brief return the smallest value
 */

#define SMALLEST(a, b) a < (b) ? (a) : b
#define HIGHEST(a, b) a > (b) ? (a) : b

extern area_t ouchat_last_areas[16];
extern area_t ouchat_areas[16];

/**
 * @brief This function is used to extract object from reads
 * @param (uint8_t) trigger_value : minimum size of objects
 * @param (double_t) input_data : data to convert
 * @param (int16_t) data_background : background to separate objects
 * @param (uint16_t) *p_output_data
 * @return (uint8_t) status : 0 if inverting is OK.
 */
uint8_t ouchat_negative_data(
        const double_t trigger_value,
        const double_t input_data[64],
        double_t data_background,
        uint16_t *p_output_data
);

/**
 * @brief This function is used to extract area from the VL53L5CX output data
 * @param (int16_t) *p_data : VL53L5CX results data.
 * @param (uint16_t) p_background : background of the reads
 * @param (void) *p_callback(double_t, double_t) : callback on cat move
 * @return (uint8_t) status : 0 if processing is OK.
 */

uint8_t ouchat_handle_data(
        const int16_t input_data[64],
        int16_t data_background,
        void (*p_callback)(double_t, double_t, area_t, area_t)
);

/**
 * @brief This function is used to get the context of the detection
 * @param (int16_t) input_data
 * @param (*int16_t) p_output_data
 * @return (uint8_t) status : 0 if processing is OK.
 */
uint8_t ouchat_get_context(
        const int16_t input_data[64],
        int16_t *p_output_data
);

/**
 * @brief Inner function, not available outside this file. This function is used
 * recursively to cut out the cat from VL53L5CX output.
 * @param (*uint8_t) p_output : output data
 * @param (uint16_t) p_input : input data
 * @param (uint8_t) i : pixel to cut out.
 * @param (uint8_t) sln : number of the selection.
 * @return (uint8_t) status : 0 if cutting is OK.
 */
static uint8_t process_cutting(
        uint8_t *p_output,
        uint16_t p_input[64],
        uint8_t i,
        uint8_t sln
);

uint8_t ouchat_processing_wakeup();

uint8_t ouchat_process_trajectory_points(area_t area1, area_t area2, uint8_t address);

#endif //OUCHAT_PROCESSOR_H_
