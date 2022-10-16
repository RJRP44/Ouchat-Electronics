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

uint8_t array_find(const uint8_t *p_array, int8_t array_size, uint8_t element, int8_t *pos) {

    for (int8_t i = 0; i < array_size; ++i) {
        if (*(p_array + i) == element) {
            *pos = i;
            return 0;
        }
    }
    return 1;
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

int area_address_compar(const void *p1, const void *p2) {

    if (ouchat_last_areas[*(uint8_t *) p1].sum == 0 && ouchat_last_areas[*(uint8_t *) p2].sum == 0) {
        return (*(uint8_t *) p1 - *(uint8_t *) p2);
    }

    return ouchat_last_areas[*(uint8_t *) p2].sum - ouchat_last_areas[*(uint8_t *) p1].sum;
}

int area_points_compar(const void *p1, const void *p2) {
    //Find the area address
    uint16_t p1_value = *(uint16_t *) p1;
    uint16_t p2_value = *(uint16_t *) p2;

    if(p1_value == 0){
        return 1;
    }

    if(p2_value == 0){
        return -1;
    }

    uint8_t address = p1_value - ((p1_value >> 4) << 4);
    area_t area = ouchat_last_areas[address];

    //Get all
    uint8_t p1_index = p1_value >> 4;
    p1_index --;
    uint8_t p2_index = p2_value >> 4;
    p2_index --;
    uint8_t center_index = POINT_TO_1D(round(area.center.x), round(area.center.y));

    double_t p1_distance = point_distance(p1_index, center_index);
    double_t p2_distance = point_distance(p2_index, center_index);

    if(p1_distance == p2_distance){
        return 0;
    }
    if(p1_distance > p2_distance){
        return 1;
    }
    if(p1_distance < p2_distance){
        return -1;
    }
    return 0;
}

int destroy_priority(uint8_t i, int8_t *priority, const uint8_t p_input[64]) {
    if(i < 0){
        *priority = -1;
        return 1;
    }
    if (p_input[i] == 16) {
        *priority = -1;
        return 1;
    }

    if (p_input[ORDINATE_FROM_1D(i) < 7 ? i + 1 : i] == 16 || ORDINATE_FROM_1D(i) >= 7) {
        *priority += 1;
    }
    if (p_input[ORDINATE_FROM_1D(i) > 0 ? i - 1 : i] == 16 || ORDINATE_FROM_1D(i) <= 0) {
        *priority += 1;
    }
    if (p_input[i < 56 ? i + 8 : i] == 16 || i >= 56) {
        *priority += 1;
    }
    if (p_input[i > 7 ? i - 8 : i] == 16 || i <= 7) {
        *priority += 1;
    }
    return 0;
}

int destroy_compar(const void *p1, const void *p2) {
    int8_t p1_p = 0;
    int8_t p2_p = 0;

    if(*(uint8_t *) p1 == 0){
        return -1;
    }

    if(*(uint8_t *) p2 == 0){
        return 1;
    }

    destroy_priority((*(uint8_t *) p1), &p1_p, ouchat_temp_data);
    destroy_priority((*(uint8_t *) p2), &p2_p, ouchat_temp_data);

    return p2_p - p1_p;
}