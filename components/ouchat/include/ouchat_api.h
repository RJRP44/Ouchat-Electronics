//
// Created by RJRP on 24/06/2022.
//

#ifndef OUCHAT_PROCESSOR_H_
#define OUCHAT_PROCESSOR_H_

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "vl53l5cx_api.h"

#define X_FROM_1D(i) (i - i % 8) / 8.00
#define Y_FROM_1D(i) i % 8

/**
 * @brief Structure cat contains coordinate of a selection in the VL53L5CX output
 * data. It contains the Top Left coordinate of the selection (p_tlc), the Bottom
 * Right coordinate of the selection (p_brc), and the Center of the selection
 * (p_c)
 */

typedef struct {
    uint8_t p_tlc;
    uint8_t p_brc;
    uint8_t p_c;
} cat;

/**
 * @brief This function is used to extract cat from the VL53L5CX output data
 * @param (int16_t) *p_data : VL53L5CX results data.
 * @param (uint16_t) p_background : background of the reads
 * @param (*uint8_t) *p_data : output data
 * @return (uint8_t) status : 0 if processing is OK.
 */

uint8_t ouchat_process_data(
        const int16_t p_data[64],
        const int16_t p_background[64],
        uint8_t *p_output
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
        int16_t p_input[64],
        uint8_t i,
        uint8_t sln
        );

/**
 * @brief Inner function, not available outside this file. This function is used
 * to calculate the difference between 2 cats.
 * @param (cat) a_cat : 1st cat to compare.
 * @param (cat) b_cat : 2nd cat to compare.
 * @param (double_t) *p_difference : value of the difference.
 */

static uint8_t cats_difference(
        cat a_cat,
        cat b_cat,
        double_t *p_difference
        );

/**
 * @brief Inner function, not available outside this file. This fuction is used
 * to calculate the distance between 2 points of a 1D array.
 * @param (uint_8) a : 1st point,
 * @param (uint_8) b : 2nd point,
 * @return (double_t) the distance between the 2 points.
 */

static double_t point_distance(
        uint8_t a,
        uint8_t b
        );

#endif //OUCHAT_PROCESSOR_H_
