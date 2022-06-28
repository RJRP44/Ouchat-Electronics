//
// Created by RJRP on 24/06/2022.
//

#include "ouchat_api.h"
#include <math.h>

cat_t lcats[17];
uint8_t lsln = 1;
cat_t start_cats[17];

uint8_t memswap(uint8_t source, uint8_t destination, cat_t *p_array) {
    cat_t temp_src = *(p_array + source);
    *(p_array + source) = *(p_array + destination);
    *(p_array + destination) = temp_src;
    return 0;
}

static double_t point_distance(
        uint8_t a,
        uint8_t b
) {
    return (double_t) sqrt(pow(X_FROM_1D(a) - X_FROM_1D(b), 2) + pow(Y_FROM_1D(a) - Y_FROM_1D(b), 2));
}

static double_t cats_difference(
        cat_t a_cat,
        cat_t b_cat
) {
    double_t difference = 0;
    difference += (double_t) sqrt(pow(a_cat.p_c.x - b_cat.p_c.x, 2) + pow(a_cat.p_c.y - b_cat.p_c.y, 2)) * 2;
    difference += point_distance(a_cat.p_brc, b_cat.p_brc);
    difference += point_distance(a_cat.p_tlc, b_cat.p_tlc);
    return difference;
}

static uint8_t process_cutting(
        uint8_t *p_output,
        int16_t p_input[64],
        uint8_t i,
        uint8_t sln
) {
    if (*(p_output + i) != sln) {
        p_output[i] = sln;

        if (p_input[Y_FROM_1D(i) < 7 ? i + 1 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) < 7 ? i + 1 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) > 0 ? i - 1 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) > 0 ? i - 1 : i, sln);
        }
        if (p_input[i < 55 ? i + 8 : i]) {
            process_cutting(p_output, p_input, i < 55 ? i + 8 : i, sln);
        }
        if (p_input[i > 8 ? i - 8 : i]) {
            process_cutting(p_output, p_input, i > 8 ? i - 8 : i, sln);
        }

        if (p_input[Y_FROM_1D(i) < 7 && i < 55 ? i + 9 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) < 7 && i < 55 ? i + 9 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) > 0 && i > 8 ? i - 9 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) > 0 && i > 8 ? i - 9 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) < 7 && i > 8 ? i - 7 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) < 7 && i > 8 ? i - 7 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) > 0 && i < 55 ? i + 7 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) > 0 && i < 55 ? i + 7 : i, sln);
        }


    }
    return 0;
}

uint8_t ouchat_process_data(
        const int16_t p_data[64],
        const int16_t p_background[64],
        uint8_t *p_output) {

    int16_t idistance[64];
    uint16_t total[3][17];
    cat_t scats[17];

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

    for (int i = 1; i < 17; ++i) {
        scats[i].p_brc = 0;
        scats[i].p_tlc = C_TO_1D(16, 16);
    }

    if (sln == 1 && lsln == 1) {
        scats[0].p_c.y = 1;
        memcpy(lcats, scats, sizeof(scats));
    }

    for (int i = 0; i < 64; ++i) {
        if (idistance[i] > 0) {
            total[0][*(p_output + i)]++;
            total[1][*(p_output + i)] += 1 + X_FROM_1D(i);
            total[2][*(p_output + i)] += 1 + Y_FROM_1D(i);

            scats[*(p_output + i)].p_tlc = X_FROM_1D(i) < X_FROM_1D(scats[*(p_output + i)].p_tlc) ?
                                           C_TO_1D(X_FROM_1D(i), Y_FROM_1D(scats[*(p_output + i)].p_tlc)) : scats[*(
                            p_output + i)].p_tlc;

            scats[*(p_output + i)].p_tlc = Y_FROM_1D(i) < Y_FROM_1D(scats[*(p_output + i)].p_tlc) ?
                                           C_TO_1D(X_FROM_1D(scats[*(p_output + i)].p_tlc), Y_FROM_1D(i)) : scats[*(
                            p_output + i)].p_tlc;

            scats[*(p_output + i)].p_brc = X_FROM_1D(i) > X_FROM_1D(scats[*(p_output + i)].p_brc) ?
                                           C_TO_1D(X_FROM_1D(i), Y_FROM_1D(scats[*(p_output + i)].p_brc)) : scats[*(
                            p_output + i)].p_brc;

            scats[*(p_output + i)].p_brc = Y_FROM_1D(i) > Y_FROM_1D(scats[*(p_output + i)].p_brc) ?
                                           C_TO_1D(X_FROM_1D(scats[*(p_output + i)].p_brc), Y_FROM_1D(i)) : scats[*(
                            p_output + i)].p_brc;
        }
    }

    for (int i = 1; i < 17; ++i) {
        if (total[0][i] > 0) {
            scats[i].p_c.x = (((double) total[1][i] / (double) total[0][i]) - 1);
            scats[i].p_c.y = (((double) total[2][i] / (double) total[0][i]) - 1);
        }
    }

    cat_t temp[17];
    memcpy(temp, scats, sizeof(temp));

#if OUCHAT_API_VERBOSE
    printf("\n");
#endif

    if (lcats[0].p_c.y != 1) {
        for (int i = 1; i < 17; ++i) {
            if (total[0][i] > 0 && lcats[i].p_tlc != C_TO_1D(16, 16)) {
                //Maximum difference is 39.598
                double_t min_diff = 80;
                uint8_t min_index = i;
                for (int j = 1; j < 16; ++j) {
                    if (lcats[j].p_tlc != C_TO_1D(16, 16)) {
                        min_index = min_diff > cats_difference(lcats[j], scats[i]) ? j : min_index;
                        min_diff = SMALLEST(min_diff, cats_difference(lcats[j], scats[i]));
#if OUCHAT_API_VERBOSE
                        printf("%d,", j);
#endif
                    }
                }

                if (temp[min_index].p_c.x != scats[i].p_c.x ||
                    temp[min_index].p_c.y != scats[i].p_c.y ||
                    temp[min_index].p_brc != scats[i].p_brc ||
                    temp[min_index].p_tlc != scats[i].p_tlc) {

                    memswap(min_index, i, temp);
#if OUCHAT_API_VERBOSE
                    printf("Swapping : src=%d dest=%d\n", min_index, i);
#endif
                } else {
#if OUCHAT_API_VERBOSE
                    printf("Try Swapping : src=%d dest=%d\n", min_index, i);
#endif
                }
            }
        }
    }
#if OUCHAT_API_VERBOSE
    for (int i = 1; i < 17; ++i) {
        if (lcats[i].p_brc > 0 || total[0][i] > 0) {
            printf("zone %d : c(%f,%f) tl(%d,%d) br(%d,%d) difference with : 1= %f, 2= %f, 3= %f\n", i, temp[i].p_c.x,
                   temp[i].p_c.y, X_FROM_1D(temp[i].p_tlc), Y_FROM_1D(temp[i].p_tlc), X_FROM_1D(temp[i].p_brc),
                   Y_FROM_1D(temp[i].p_brc),
                   cats_difference(lcats[1], temp[i]), cats_difference(lcats[2], temp[i]),
                   cats_difference(lcats[3], temp[i]));
        }
    }
#endif

    for (int i = 1; i < 17; ++i) {
        if(lcats[i].p_tlc == C_TO_1D(16, 16) && temp[i].p_tlc != C_TO_1D(16,16)){
            start_cats[i] = temp[i];
#if OUCHAT_API_VERBOSE
            printf("Start of %d\n", i);
#endif
        }
        if(lcats[i].p_tlc != C_TO_1D(16, 16) && temp[i].p_tlc == C_TO_1D(16,16)){
#if OUCHAT_API_VERBOSE
            printf("End of %d\n", i);
            printf("Selection %d moved x:%f y:%f\n", i , start_cats[i].p_c.x - lcats[i].p_c.x, start_cats[i].p_c.y - lcats[i].p_c.y);
#endif
        }
    }
/*
    if (total[0][1] > 0) {
        printf("p_c(%f,%f) p_tlc(%d,%d) p_brc(%d,%d)\n", scats[1].p_c.x, scats[1].p_c.y,
               X_FROM_1D(scats[1].p_tlc), Y_FROM_1D(scats[1].p_tlc),
               X_FROM_1D(scats[1].p_brc), Y_FROM_1D(scats[1].p_brc));

        printf("1-1 : %f, 1-2 : %f, 2-1 : %f, 2-2 : %f,", cats_difference(lcats[1], scats[1]),
               cats_difference(lcats[1], scats[2]), cats_difference(lcats[2], scats[1]),
               cats_difference(lcats[2], scats[2]));
    }*/


    memcpy(lcats, temp, sizeof(temp));
    lsln = sln;
    return 0;
}