//
// Created by RJRP on 24/06/2022.
//

#include "ouchat_api.h"
#include <math.h>

static double_t point_distance(
        uint8_t a,
        uint8_t b
){
    return (double_t) sqrt(pow(X_FROM_1D(a) - X_FROM_1D(b) , 2) + pow(Y_FROM_1D(a) - Y_FROM_1D(b) , 2));
}

static uint8_t cats_difference(
        cat a_cat,
        cat b_cat,
        double_t *p_difference
){
    *p_difference += point_distance(a_cat.p_c, b_cat.p_c) * 2;
    *p_difference += point_distance(a_cat.p_brc, b_cat.p_brc);
    *p_difference += point_distance(a_cat.p_tlc, b_cat.p_tlc);
}

static uint8_t process_cutting(
        uint8_t *p_output,
        int16_t p_input[64],
        uint8_t i,
        uint8_t sln
) {
    if (*(p_output + i) != sln) {
        p_output[i] = sln;

        if (p_input[i % 8 < 7 ? i + 1 : i]) {
            process_cutting(p_output, p_input, i % 8 < 7 ? i + 1 : i, sln);
        }
        if (p_input[i % 8 > 0 ? i - 1 : i]) {
            process_cutting(p_output, p_input, i % 8 > 0 ? i - 1 : i, sln);
        }
        if (p_input[i < 55 ? i + 8 : i]) {
            process_cutting(p_output, p_input, i < 55 ? i + 8 : i, sln);
        }
        if (p_input[i > 8 ? i - 8 : i]) {
            process_cutting(p_output, p_input, i > 8 ? i - 8 : i, sln);
        }

        if (p_input[i % 8 < 7 && i < 55 ? i + 9 : i]) {
            process_cutting(p_output, p_input, i % 8 < 7 && i < 55 ? i + 9 : i, sln);
        }
        if (p_input[i % 8 > 0 && i > 8 ? i - 9 : i]) {
            process_cutting(p_output, p_input, i % 8 > 0 && i > 8 ? i - 9 : i, sln);
        }
        if (p_input[i % 8 < 7 && i > 8 ? i - 7 : i]) {
            process_cutting(p_output, p_input, i % 8 < 7 && i > 8 ? i - 7 : i, sln);
        }
        if (p_input[i % 8 > 0 && i < 55 ? i + 7 : i]) {
            process_cutting(p_output, p_input, i % 8 > 0 && i < 55 ? i + 7 : i, sln);
        }


    }
    return 0;
}

uint8_t ouchat_process_data(
        const int16_t p_data[64],
        const int16_t p_background[64],
        uint8_t *p_output) {

    int16_t idistance[64];
    uint16_t total[3][16];
    memset(total, 0, sizeof total);

    for (int i = 0; i < 64; ++i) {
        idistance[i] = p_background[i] - p_data[i] - 200;
        if (idistance[i] < 0) {
            idistance[i] = 0;
        }
    }

    uint8_t sln = 1;
    for (int i = 0; i < 64; ++i) {
        if (idistance[i] > 0 && *(p_output + i) == 0) {
            process_cutting(p_output, idistance, i, sln);
            sln++;
        }
    }

    for (int i = 0; i < 64; ++i) {
        if (idistance[i] > 0) {
            total[0][*(p_output + i)] ++;
            total[1][*(p_output + i)] += 1 + (i - i % 8) / 8;
            total[2][*(p_output + i)] += 1 + (i % 8);
        }
    }

    if (total[0][1] > 0) {
        printf("(%f,%f) \n", ((double)total[1][1]/(double)total[0][1])-1,((double)total[2][1]/(double)total[0][1])-1);
    }

    return 0;
}