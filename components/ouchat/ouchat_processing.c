//
// Created by RJRP on 24/06/2022.
//

#include <math.h>
#include <stdlib.h>
#include "ouchat_processing.h"
#include "ouchat_utils.h"

area_t ouchat_last_areas[16];
area_t ouchat_areas[16];

uint16_t last_sum_data[3][16];
uint8_t last_address = 0x01;
area_t start_cats[17];

static int8_t trajectories[16][64];

uint8_t ouchat_negative_data(
        const double_t trigger_value,
        const double_t input_data[64],
        double_t data_background,
        uint16_t *p_output_data
) {
    for (int i = 0; i < 64; ++i) {
        *(p_output_data + i) = 0;
        if (data_background - input_data[i] > trigger_value) {
            *(p_output_data + i) = data_background - input_data[i] - trigger_value;
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
uint8_t ouchat_get_context(
        const int16_t input_data[64],
        int16_t *p_output_data
) {
    double_t total = 0;
    uint8_t shift_address = 0;

    //Correct all values
    for (int i = 0; i < 64; ++i) {
        if(input_data[i] < 90){
            total += 0;
            ++shift_address;
            continue;
        }
        total += input_data[i] * cos((3 + 10.3125 + 5.625 * (i % 8)) * PI / 180.0);
    }

    *p_output_data = (int16_t) (total / (64 - shift_address));

    return 0;
}

uint8_t ouchat_handle_data(
        const int16_t input_data[64],
        int16_t data_background,
        void (*p_callback)(double_t, double_t, area_t, area_t)
) {

    uint16_t negative_data[64];
    uint8_t area_address[64];
    uint16_t sum_data[3][16];

    //Set all elements of the array to 0
    memset(sum_data, 0, sizeof sum_data);
    memset(area_address, 16, sizeof area_address);
    memset(ouchat_areas, 0, sizeof(ouchat_areas));

    for (int i = 0; i < 16; ++i) {
        ouchat_areas[i].sum = 0;
    }

    double_t temp_input_data[64];

    //Correct all values
    for (int i = 0; i < 64; ++i) {
        if(input_data[i] < 90){
            temp_input_data[i] = data_background;
            continue;
        }
        temp_input_data[i] = input_data[i] * cos((3 + 10.3125 + 5.625 * (i % 8)) * PI / 180.0);
    }

    //Get all the values above the floor
    ouchat_negative_data(150.00, temp_input_data, data_background, &negative_data[0]);

    uint8_t remaining_address[16];
    uint8_t shift_address = 0;

    //Init all address
    for (int i = 0; i < 16; ++i) {
        remaining_address[i] = i;
    }

    qsort(remaining_address, 16, sizeof(uint8_t), area_address_compar);

    for(int i = 0; i < 16; ++i){
        if(ouchat_last_areas[i].size > 0){
            qsort(ouchat_last_areas[i].indexes,64,sizeof(uint16_t),area_points_compar);
        }
    }

    //First go through all last address
    for (int i = 0; i < 16; ++i) {
        uint8_t address = remaining_address[i];
        area_t area = ouchat_last_areas[address];

        if(area.size > 0 && remaining_address[i] < 16){
#if OUCHAT_API_VERBOSE
            printf("\n[%d]",address);
#endif
            uint8_t found = 0;

            for (int j = 0; j < 64 && area.indexes[j] >> 4 != 0 && found == 0; ++j) {

                uint8_t index = (area.indexes[j] >> 4) - 1;
#if OUCHAT_API_VERBOSE
                printf("%d,",index);
#endif
                if(negative_data[index] > 0 && area_address[index] == 16){
                    process_cutting(&area_address[0], negative_data, index, address);

                    found = 1;
                }
            }

            if(found == 0 && trajectories[address][0] != -1){
                for (int j = 0; j < 64 && trajectories[address][j] != -1 && found == 0; ++j) {

                    int8_t index = trajectories[address][j];
#if OUCHAT_API_VERBOSE
                    printf("/%d/,",index);
#endif

                    if(negative_data[index] > 0 && area_address[index] == 16){
                        process_cutting(&area_address[0], negative_data, index, address);

                        found = 1;
                    }
                }
            }

            memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
            remaining_address[15] = 16;
            --i;

            if(found == 0){
                ++shift_address;
            }
        }
    }

    //Temporarily addressing all areas
    for (int i = 0; i < 64; ++i) {
        if (negative_data[i] > 0 && area_address[i] == 16) {
#if OUCHAT_API_VERBOSE
            printf("changing on %d, on %d (%d;%d) a : %d", remaining_address[0], i, ABSCISSA_FROM_1D(i),
                   ORDINATE_FROM_1D(i), area_address[i]);
#endif
            process_cutting(&area_address[0], negative_data, i, remaining_address[0]);
            memmove(remaining_address, remaining_address + 1, 15 * sizeof(uint8_t));
            remaining_address[15] = 16;
        }
    }

    //Init all areas
    for (int i = 0; i < 16; ++i) {
        ouchat_areas[i].bottom_right = 0;
        ouchat_areas[i].top_left = POINT_TO_1D(16, 16);
    }

    //Checks if 0 zones have been detected
    if (remaining_address[15 - shift_address] != 16 && last_address != 16) {
        memcpy(ouchat_last_areas, ouchat_areas, sizeof(ouchat_areas));
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

            area_t area = ouchat_areas[address];

            ouchat_areas[address].indexes[area.size] = ((i + 1) << 4 ) + address;
            ouchat_areas[address].size ++;
#if OUCHAT_API_VERBOSE
            printf("[%d]%d(%d),",address,((i + 1) << 4 ) + address,ouchat_areas[address].indexes[area.size]);
#endif

            if (ABSCISSA_FROM_1D(i) < ABSCISSA_FROM_1D(ouchat_areas[address].top_left)) {
                ouchat_areas[address].top_left = POINT_TO_1D(ABSCISSA_FROM_1D(i), ORDINATE_FROM_1D(ouchat_areas[address].top_left));
            }

            if (ORDINATE_FROM_1D(i) < ORDINATE_FROM_1D(ouchat_areas[address].top_left)) {
                ouchat_areas[address].top_left = POINT_TO_1D(ABSCISSA_FROM_1D(ouchat_areas[address].top_left), ORDINATE_FROM_1D(i));
            }

            if (ABSCISSA_FROM_1D(i) > ABSCISSA_FROM_1D(ouchat_areas[address].bottom_right)) {
                ouchat_areas[address].bottom_right =
                        POINT_TO_1D(ABSCISSA_FROM_1D(i), ORDINATE_FROM_1D(ouchat_areas[address].bottom_right));
            }

            if (ORDINATE_FROM_1D(i) > ORDINATE_FROM_1D(ouchat_areas[address].bottom_right)) {
                ouchat_areas[address].bottom_right =
                        POINT_TO_1D(ABSCISSA_FROM_1D(ouchat_areas[address].bottom_right), ORDINATE_FROM_1D(i));
            }
        }
    }

    //Get the left part of each area
    for (int i = 0; i < 16; ++i) {
        uint16_t left_sum[2];
        memset(left_sum, 0, sizeof left_sum);
        for (int j = 0; j < 64; ++j) {
            if (area_address[j] == i) {
                for (int k = 0; k < 8; ++k) {
                    uint8_t address = POINT_TO_1D(k, ORDINATE_FROM_1D(j));
                    if (area_address[address] == i) {
                        left_sum[1] += 1 + k;
                        left_sum[TOTAL_SUM]++;
                    }
                }
                ouchat_areas[i].left_center.y = ORDINATE_FROM_1D(j);
                ouchat_areas[i].left_center.x = (double_t) left_sum[1] / left_sum[TOTAL_SUM] - 1;
                break;
            }
        }
    }

    //Calculates center of areas
    for (int i = 0; i < 16; ++i) {
        if (sum_data[TOTAL_SUM][i] > 0) {
            ouchat_areas[i].center.x = (double_t) sum_data[ABSCISSA_SUM][i] / sum_data[TOTAL_SUM][i] - 1;
            ouchat_areas[i].center.y = (double_t) sum_data[ORDINATE_SUM][i] / sum_data[TOTAL_SUM][i] - 1;
        }
        ouchat_areas[i].sum = sum_data[TOTAL_SUM][i];
    }

    area_t temp_areas[16];
    memcpy(temp_areas, ouchat_areas, sizeof(temp_areas));

#if OUCHAT_API_VERBOSE
    //Only for debugging
    for (int i = 0; i < 16; ++i) {
        if (ouchat_last_areas[i].bottom_right > 0 || sum_data[TOTAL_SUM][i] > 0) {

            printf("\nzone %d : c : %f c(%f,%f) tl(%d,%d) br(%d,%d) cl(%f,%f) sum=%d difference with : 1= %f, 2= %f, 3= %f [", i,
                   (POINT_TO_1D(round(ouchat_areas[i].center.x), round(ouchat_areas[i].center.y))),
                   temp_areas[i].center.x,
                   temp_areas[i].center.y,
                   ABSCISSA_FROM_1D(temp_areas[i].top_left),
                   ORDINATE_FROM_1D(temp_areas[i].top_left),
                   ABSCISSA_FROM_1D(temp_areas[i].bottom_right),
                   ORDINATE_FROM_1D(temp_areas[i].bottom_right),
                   temp_areas[i].left_center.x,
                   temp_areas[i].left_center.y,
                   sum_data[TOTAL_SUM][i],
                   area_difference(ouchat_last_areas[1], temp_areas[i]),
                   area_difference(ouchat_last_areas[2], temp_areas[i]),
                   area_difference(ouchat_last_areas[3], temp_areas[i]));

            for (int j = 0; j < 64; ++j) {
                printf("%d,",(ouchat_last_areas[i].indexes[j] >> 4) - 1);
            }
            printf("]\n");
        }
    }
#endif

    for (int i = 0; i < 16; ++i) {
        if (ouchat_last_areas[i].top_left == C_TO_1D(16, 16) && temp_areas[i].top_left != C_TO_1D(16, 16)) {
            start_cats[i] = temp_areas[i];
#if OUCHAT_API_VERBOSE
            printf("Start of %d\n", i);
#endif
        }
        if (ouchat_last_areas[i].top_left != C_TO_1D(16, 16) && temp_areas[i].top_left == C_TO_1D(16, 16)) {
#if OUCHAT_API_VERBOSE
            printf("End of %d\n", i);
            printf("Selection %d moved x:%f y:%f\n", i,
                   start_cats[i].center.x - ouchat_last_areas[i].center.x,
                   start_cats[i].center.y - ouchat_last_areas[i].center.y);
#endif
            (*p_callback)(start_cats[i].center.x - ouchat_last_areas[i].center.x,
                          start_cats[i].center.y - ouchat_last_areas[i].center.y,
                          start_cats[i], ouchat_last_areas[i]);

        }
    }
#if OUCHAT_API_VERBOSE
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
    memset(trajectories, -1, sizeof(trajectories));
    return 0;
}

uint8_t ouchat_processing_wakeup(){
    //Init all last areas
    for (int i = 0; i < 16; ++i) {
        ouchat_last_areas[i].bottom_right = 0;
        ouchat_last_areas[i].top_left = POINT_TO_1D(16, 16);
    }

    return 0;
}

uint8_t ouchat_process_trajectory_points(area_t area1, area_t area2, uint8_t address){

    int8_t temp_trajectory[64];
    memset(temp_trajectory, -1, sizeof(temp_trajectory));

    int8_t *temp_point = &temp_trajectory[0];


    double_t b = -(area2.center.x - area1.center.x);
    double_t a = area2.center.y - area1.center.y;
    double_t c = -(a*area1.center.x + b*area1.center.y);

    for (int8_t i = 0; i < 64; ++i) {
        uint8_t x = ABSCISSA_FROM_1D(i);
        uint8_t y = ORDINATE_FROM_1D(i);

        long double distance = fabsl(a * x + b * y + c) / sqrtl(powl(a,2) + powl(b,2));

        if(distance <= 0.5){
            *temp_point = i;
            temp_point++;
        }
    }

    qsort(temp_trajectory, temp_point-temp_trajectory, sizeof(int8_t), point_distance_compr);

    memcpy(trajectories[address], temp_trajectory, sizeof(temp_trajectory));
#if OUCHAT_API_VERBOSE
    printf("T%i[",address);
    for (int i = 0; i < 64; ++i) {
        printf("%i,",trajectories[address][i]);
    }
    printf("]\n");
#endif
    return 0;
}