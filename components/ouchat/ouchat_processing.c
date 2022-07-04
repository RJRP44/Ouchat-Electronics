//
// Created by RJRP on 24/06/2022.
//

#include <math.h>
#include <stdlib.h>
#include "ouchat_processing.h"
#include "ouchat_utils.h"

area_t ouchat_last_areas[16];
uint16_t last_sum_data[3][16];
uint8_t last_address = 0x01;
area_t start_cats[17];

uint8_t ouchat_negative_data(
        const uint8_t trigger_value,
        const int16_t input_data[64],
        const int16_t data_background[64],
        uint16_t *p_output_data
) {
    for (int i = 0; i < 64; ++i) {
        *(p_output_data + i) = 0;
        if (data_background[i] - input_data[i] > trigger_value) {
            *(p_output_data + i) = data_background[i] - input_data[i] - trigger_value;
        }
    }
    return 0;
}

static uint8_t process_cutting(
        uint8_t *p_output,
        uint16_t p_input[64],
        uint8_t i,
        uint8_t sln
) {
    if (*(p_output + i) != sln && *(p_output + i) == 16) {
        p_output[i] = sln;

        if (p_input[Y_FROM_1D(i) < 7 ? i + 1 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) < 7 ? i + 1 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) > 0 ? i - 1 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) > 0 ? i - 1 : i, sln);
        }
        if (p_input[i < 56 ? i + 8 : i]) {
            process_cutting(p_output, p_input, i < 56 ? i + 8 : i, sln);
        }
        if (p_input[i > 7 ? i - 8 : i]) {
            process_cutting(p_output, p_input, i > 7 ? i - 8 : i, sln);
        }

        if (p_input[Y_FROM_1D(i) < 7 && i < 56 ? i + 9 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) < 7 && i < 56 ? i + 9 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) > 0 && i > 7 ? i - 9 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) > 0 && i > 7 ? i - 9 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) < 7 && i > 7 ? i - 7 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) < 7 && i > 7 ? i - 7 : i, sln);
        }
        if (p_input[Y_FROM_1D(i) > 0 && i < 56 ? i + 7 : i]) {
            process_cutting(p_output, p_input, Y_FROM_1D(i) > 0 && i < 56 ? i + 7 : i, sln);
        }


    }
    return 0;
}

uint8_t ouchat_handle_data(
        const int16_t input_data[64],
        const int16_t data_background[64],
        void (*p_callback)(double_t, double_t, area_t, area_t)
) {

    uint16_t negative_data[64];
    uint8_t area_address[64];
    uint16_t sum_data[3][16];

    area_t areas[16];

    //Set all elements of the array to 0
    memset(sum_data, 0, sizeof sum_data);
    memset(area_address, 16, sizeof area_address);
    for (int i = 0; i < 16; ++i) {
        areas[i].sum = 0;
    }

    //Get all the values above the floor
    ouchat_negative_data(200, input_data, data_background, &negative_data[0]);

    uint8_t remaining_address[16];
    uint8_t shift_address = 0;

    //Init all address
    for (int i = 0; i < 16; ++i) {
        remaining_address[i] = i;
    }

    qsort(remaining_address, 16, sizeof(uint8_t), area_address_compar);

    //First go through all last address
    for (int i = 0; i < 16; ++i) {
        uint8_t address = remaining_address[i];

        if (last_sum_data[TOTAL_SUM][address] > 0 && remaining_address[i] < 16) {
            u_int8_t center_index =
                    POINT_TO_1D(round(ouchat_last_areas[address].center.x), round(ouchat_last_areas[address].center.y));

            if (negative_data[center_index] > 0 && area_address[center_index] == 16) {

#if OUCHAT_API_VERBOSE
                printf("Address : %d recovering on %d (%f;%f)\n",
                       address,
                       center_index,
                       round(ouchat_last_areas[address].center.x),
                       round(ouchat_last_areas[address].center.y));
#endif

                process_cutting(&area_address[0], negative_data, center_index, address);

                memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
                remaining_address[15] = 16;
                --i;
            } else if (negative_data[Y_FROM_1D(center_index) < 7 ? center_index + 1 : center_index] &&
                       area_address[Y_FROM_1D(center_index) < 7 ? center_index + 1 : center_index] == 16) {

                process_cutting(&area_address[0], negative_data,
                                Y_FROM_1D(center_index) < 7 ? center_index + 1 : center_index, address);

#if OUCHAT_API_VERBOSE
                printf("Address : %d recovering on %d (%f;%f) y+1\n",
                       address,
                       Y_FROM_1D(center_index) < 7 ? center_index + 1 : center_index,
                       round(ouchat_last_areas[address].center.x),
                       round(ouchat_last_areas[address].center.y + 1));
#endif
                memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
                remaining_address[15] = 16;
                --i;

            } else if (negative_data[Y_FROM_1D(center_index) > 0 ? center_index - 1 : center_index] &&
                       area_address[Y_FROM_1D(center_index) > 0 ? center_index - 1 : center_index] == 16) {

                process_cutting(&area_address[0], negative_data,
                                Y_FROM_1D(center_index) > 0 ? center_index - 1 : center_index, address);

#if OUCHAT_API_VERBOSE
                printf("Address : %d recovering on %d (%f;%f) y-1\n",
                       address,
                       Y_FROM_1D(center_index) > 0 ? center_index - 1 : center_index,
                       round(ouchat_last_areas[address].center.x),
                       round(ouchat_last_areas[address].center.y - 1));
#endif
                memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
                remaining_address[15] = 16;
                --i;

            } else if (negative_data[center_index < 55 ? center_index + 8 : center_index] &&
                       area_address[center_index < 55 ? center_index + 8 : center_index] == 16) {

                process_cutting(&area_address[0], negative_data, center_index < 55 ? center_index + 8 : center_index,
                                address);

#if OUCHAT_API_VERBOSE
                printf("Address : %d recovering on %d (%f;%f) x+1\n",
                       address,
                       center_index < 55 ? center_index + 8 : center_index,
                       round(ouchat_last_areas[address].center.x + 1),
                       round(ouchat_last_areas[address].center.y));
#endif
                memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
                remaining_address[15] = 16;
                --i;

            } else if (negative_data[center_index > 8 ? center_index - 8 : center_index] &&
                       area_address[center_index > 8 ? center_index - 8 : center_index] == 16) {

                process_cutting(&area_address[0], negative_data, center_index > 8 ? center_index - 8 : center_index,
                                address);

#if OUCHAT_API_VERBOSE
                printf("Address : %d recovering on %d (%f;%f) x-1\n",
                       address,
                       center_index > 8 ? center_index - 8 : center_index,
                       round(ouchat_last_areas[address].center.x - 1),
                       round(ouchat_last_areas[address].center.y));
#endif
                memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
                remaining_address[15] = 16;
                --i;

            } else {
                printf("/!\\ Address : %d couldn't recover %d (%f;%f) in loop %d\n",
                       address,
                       center_index,
                       round(ouchat_last_areas[address].center.x),
                       round(ouchat_last_areas[address].center.y),
                       i);

                memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
                remaining_address[15] = 16;
                --i;
                ++shift_address;
            }
        }
    }

    //Temporarily addressing all areas
    for (int i = 0; i < 64; ++i) {
        if (negative_data[i] > 0 && area_address[i] == 16) {
            printf("changing on %d, on %d (%d;%d) a : %d", remaining_address[0], i, ABSCISSA_FROM_1D(i), ORDINATE_FROM_1D(i), area_address[i]);
            process_cutting(&area_address[0], negative_data, i, remaining_address[0]);
            memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
            remaining_address[15] = 16;
        }
    }

    //Init all areas
    for (int i = 0; i < 16; ++i) {
        areas[i].bottom_right = 0;
        areas[i].top_left = POINT_TO_1D(16, 16);
    }

    //Checks if 0 zones have been detected
    if (remaining_address[15 - shift_address] != 16 && last_address != 16) {
        memcpy(ouchat_last_areas, areas, sizeof(areas));
        last_address = remaining_address[15 - shift_address];
        return 1;
    }

#if OUCHAT_API_VERBOSE
    for (int i = 0; i < 16; ++i) {
        printf("%d,", remaining_address[i]);
    }
    printf("\n");
#endif

    last_address = remaining_address[15];

    //Calculates point of areas
    for (int i = 0; i < 64; ++i) {
        if (negative_data[i] > 0) {

            uint8_t address = area_address[i];

            sum_data[TOTAL_SUM][address]++;
            sum_data[ABSCISSA_SUM][address] += 1 + ABSCISSA_FROM_1D(i);
            sum_data[ORDINATE_SUM][address] += 1 + ORDINATE_FROM_1D(i);

            if (ABSCISSA_FROM_1D(i) < ABSCISSA_FROM_1D(areas[address].top_left)) {
                areas[address].top_left = POINT_TO_1D(ABSCISSA_FROM_1D(i), ORDINATE_FROM_1D(areas[address].top_left));
            }

            if (ORDINATE_FROM_1D(i) < ORDINATE_FROM_1D(areas[address].top_left)) {
                areas[address].top_left = POINT_TO_1D(ABSCISSA_FROM_1D(areas[address].top_left), ORDINATE_FROM_1D(i));
            }

            if (ABSCISSA_FROM_1D(i) > ABSCISSA_FROM_1D(areas[address].bottom_right)) {
                areas[address].bottom_right =
                        POINT_TO_1D(ABSCISSA_FROM_1D(i), ORDINATE_FROM_1D(areas[address].bottom_right));
            }

            if (ORDINATE_FROM_1D(i) > ORDINATE_FROM_1D(areas[address].bottom_right)) {
                areas[address].bottom_right =
                        POINT_TO_1D(ABSCISSA_FROM_1D(areas[address].bottom_right), ORDINATE_FROM_1D(i));
            }
        }
    }

    //Calculates center of areas
    for (int i = 0; i < 16; ++i) {
        if (sum_data[TOTAL_SUM][i] > 0) {
            areas[i].center.x = (double_t) sum_data[ABSCISSA_SUM][i] / sum_data[TOTAL_SUM][i] - 1;
            areas[i].center.y = (double_t) sum_data[ORDINATE_SUM][i] / sum_data[TOTAL_SUM][i] - 1;
        }
        areas[i].sum = sum_data[TOTAL_SUM][i];
    }

    area_t temp_areas[16];
    memcpy(temp_areas, areas, sizeof(temp_areas));

#if OUCHAT_API_VERBOSE
    //Only for debugging
    for (int i = 0; i < 16; ++i) {
        if (ouchat_last_areas[i].bottom_right > 0 || sum_data[TOTAL_SUM][i] > 0) {

            printf("zone %d : c(%f,%f) tl(%d,%d) br(%d,%d) sum=%d difference with : 1= %f, 2= %f, 3= %f\n", i,
                   temp_areas[i].center.x,
                   temp_areas[i].center.y,
                   ABSCISSA_FROM_1D(temp_areas[i].top_left),
                   ORDINATE_FROM_1D(temp_areas[i].top_left),
                   ABSCISSA_FROM_1D(temp_areas[i].bottom_right),
                   ORDINATE_FROM_1D(temp_areas[i].bottom_right),
                   sum_data[TOTAL_SUM][i],
                   area_difference(ouchat_last_areas[1], temp_areas[i]),
                   area_difference(ouchat_last_areas[2], temp_areas[i]),
                   area_difference(ouchat_last_areas[3], temp_areas[i]));
        }
    }


    for (int i = 0; i < 16; ++i) {
        if (ouchat_last_areas[i].top_left == C_TO_1D(16, 16) && temp_areas[i].top_left != C_TO_1D(16, 16)) {
            start_cats[i] = temp_areas[i];

            printf("Start of %d\n", i);

        }
        if (ouchat_last_areas[i].top_left != C_TO_1D(16, 16) && temp_areas[i].top_left == C_TO_1D(16, 16)) {

            printf("End of %d\n", i);
            printf("Selection %d moved x:%f y:%f\n", i,
                   start_cats[i].center.x - ouchat_last_areas[i].center.x,
                   start_cats[i].center.y - ouchat_last_areas[i].center.y);
            (*p_callback)(start_cats[i].center.x - ouchat_last_areas[i].center.x,
                          start_cats[i].center.y - ouchat_last_areas[i].center.y,
                          start_cats[i],ouchat_last_areas[i]);

        }
    }

    for (int j = 0; j < 64; ++j) {
        if (j % 8 == 0) {
            printf("\n");
        }
        if (negative_data[j] == 0) {
            printf("  ");
        } else {
            printf("%d ", area_address[j]);
        }

    }
    printf("\n");

#endif
    memcpy(last_sum_data, sum_data, sizeof(sum_data));
    memcpy(ouchat_last_areas, temp_areas, sizeof(temp_areas));
    return 0;
}