//
// Created by RJRP on 28/06/2022.
//

#ifndef OUCHAT_ELECTRONICS_OUCHAT_UTILS_H
#define OUCHAT_ELECTRONICS_OUCHAT_UTILS_H

#include <stdint.h>
#include <math.h>

#define ABSCISSA_FROM_1D(i) (uint8_t) floor(i / 8.00)
#define ORDINATE_FROM_1D(i) i % 8
#define POINT_TO_1D(x, y) 8*(x)+(y)

/**
 * @brief Structure point contains a coordinate.
 */

typedef struct {
    double_t x;
    double_t y;
} point_t;


/**
 * @brief Structure area contains coordinate of a selection in the VL53L5CX output
 * data. It contains the Top Left coordinate of the selection (p_tlc), the Bottom
 * Right coordinate of the selection (p_brc), and the Center of the selection
 * (p_c)
 */

typedef struct {
    uint8_t top_left;
    uint8_t bottom_right;
    point_t center;
    uint8_t sum;
    point_t left_center;
} area_t;

/**
 * @brief This function is used to swap to elements in an array
 * @param (uint8_t) source : index of the source
 * @param (uint8_t) destination : index of the destination
 * @param (*area_t) *p_array : array to edit
 * @return (uint8_t) status : 0 if swapping is OK.
 */

uint8_t memory_swap(
        uint8_t source,
        uint8_t destination,
        area_t *p_array
);

/**
 * @brief This fuction is used to calculate the distance between
 * 2 points of a 1D array.
 * @param (uint_8) a : 1st point,
 * @param (uint_8) b : 2nd point,
 * @return (double_t) the distance between the 2 points.
 */

double_t point_distance(
        uint8_t a,
        uint8_t b
);

/**
 * @brief This function is used to calculate the difference
 * between 2 areas.
 * @param (area_t) a_area : 1st area to compare.
 * @param (area_t) b_area : 2nd area to compare.
 * @return (double_t) value of the difference.
 */

double_t area_difference(
        area_t a_cat,
        area_t b_cat
);

/**
 * @brief This function is used to calculate the difference
 * between 2 areas address for qsort.
 * @param (const void*) p1 : 1st address to compare
 * @param (const void*) p2 : 2nd address to compare
 * @return (int) <0 : if p1 is bigger than p2,
 *                0 : if p1 equals p2,
 *               >0 : if p1 is smaller than p2.
 */
int area_address_compar (const void* p1, const void* p2);

#endif //OUCHAT_ELECTRONICS_OUCHAT_UTILS_H
