//
// Created by RJRP on 28/06/2022.
//

#include "ouchat_utils.h"
#include "ouchat_processing.h"

uint8_t memory_swap(uint8_t source, uint8_t destination, area_t *p_array) {
    area_t temp_source = *(p_array + source);
    *(p_array + source) = *(p_array + destination);
    *(p_array + destination) = temp_source;
    return 0;
}

double_t point_distance(
        uint8_t a,
        uint8_t b
) {

    return (double_t) sqrt(pow(ABSCISSA_FROM_1D(a) - ABSCISSA_FROM_1D(b), 2) +
                           pow(ORDINATE_FROM_1D(a) - ORDINATE_FROM_1D(b), 2));
}

double_t area_difference(
        area_t a_cat,
        area_t b_cat
) {
    double_t temp_difference =
            (double_t) sqrt(pow(a_cat.center.x - b_cat.center.x, 2) + pow(a_cat.center.y - b_cat.center.y, 2)) * 2;
    temp_difference += point_distance(a_cat.bottom_right, b_cat.bottom_right);
    temp_difference += point_distance(a_cat.top_left, b_cat.top_left);
    return temp_difference;
}

int area_address_compar (const void* p1, const void* p2){

    if(ouchat_last_areas[*(uint8_t *)p1].sum == 0 && ouchat_last_areas[*(uint8_t *)p2].sum == 0 ){
        return ( *(uint8_t*)p1 - *(uint8_t*)p2 );
    }

    return ouchat_last_areas[*(uint8_t *)p2].sum - ouchat_last_areas[*(uint8_t *)p1].sum;
}