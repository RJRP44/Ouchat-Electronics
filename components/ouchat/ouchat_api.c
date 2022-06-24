//
// Created by RJRP on 24/06/2022.
//

#include "ouchat_api.h"

static uint8_t process_cutting(
        uint8_t *p_output,
        int16_t p_input[64],
        uint8_t i,
        uint8_t sln
) {
    if (*(p_output +i) != sln) {
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
    }
    return 0;
}

uint8_t ouchat_process_data(
        int16_t p_data[64],
        const int16_t p_background[64],
        uint8_t *p_output) {

    int16_t idistance[64];

    for (int i = 0; i < 64; ++i) {
        idistance[i] = p_background[i] - p_data[i] - 100;
        if(idistance[i] < 0){
            idistance[i] = 0;
        }
    }

    uint8_t sln = 1;
    for (int i = 0; i < 64; ++i) {
        if(idistance[i] > 0 && *(p_output + i) == 0){
            process_cutting(p_output, idistance, i, sln);
            sln ++;
        }
    }
    return 0;
}
