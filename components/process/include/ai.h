//
// Created by Romain on 09/03/2024.
//

#ifndef AI_H
#define AI_H

#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <vector>
#include <esp_err.h>
#include "utils.h"

#define TENSOR_ARENA_SIZE (2 * 1024)

using namespace ouchat;

namespace ouchat::ai {
    struct model_input {
        float start_x;
        float start_y;
        float start_z;
        float end_x;
        float end_y;
        float end_z;
        float displacement_x;
        float displacement_y;
        float displacement_z;
        float average_height;
        float average_deviation;
        float floor_distance;
        float absolute_displacement_x;
        float absolute_displacement_y;
        float average_size;
        float entry_box_max_x;
        float entry_box_max_y;
        float entry_box_min_x;
        float entry_box_min_y;
        float exit_box_max_x;
        float exit_box_max_y;
        float exit_box_min_x;
        float exit_box_min_y;

        [[nodiscard]] std::vector<float> to_normalized_array() const {
            std::vector<float> input;
            input.resize(23);
            input[0] = utils::normalize(start_x,-949,949);
            input[1] = utils::normalize(start_y, 0, 1256);
            input[2] = utils::normalize(start_z, 0, 800);
            input[3] = utils::normalize(end_x,-949,949);
            input[4] = utils::normalize(end_y, 0, 1256);
            input[5] = utils::normalize(end_z,0,800);
            input[6] = utils::normalize(displacement_x,-1898,1898);
            input[7] = utils::normalize(displacement_y, -1256, 1256);
            input[8] = utils::normalize(displacement_z, -800, 800);
            input[9] = utils::normalize(average_height, -800, 800);
            input[10] = utils::normalize(average_deviation, 0, 800);
            input[11] = utils::normalize(floor_distance, 600, 800);
            input[12] = utils::normalize(absolute_displacement_x,-1898,1898);
            input[13] = utils::normalize(absolute_displacement_y, -1256, 1256);
            input[14] = utils::normalize(average_size, 1, 64);
            input[15] = utils::normalize(entry_box_max_x,-949,949);
            input[16] = utils::normalize(entry_box_max_y, 0, 1256);
            input[17] = utils::normalize(entry_box_min_x,-949,949);
            input[18] = utils::normalize(entry_box_min_y, 0, 1256);
            input[19] = utils::normalize(exit_box_max_x,-949,949);
            input[20] = utils::normalize(exit_box_max_y, 0, 1256);
            input[21] = utils::normalize(exit_box_min_x,-949,949);
            input[22] = utils::normalize(exit_box_min_y, 0, 1256);
            return input;
        }
    };

    enum result {
        INSIDE = 1, OUTSIDE = -1, OTHER = 0
    };

    class interpreter {

    private:
        static uint8_t tensor_arena[TENSOR_ARENA_SIZE];
        static tflite::MicroInterpreter *micro_interpreter;

    public :
        static esp_err_t init(const void *ai_model);
        static esp_err_t predict(model_input input, result *output);
    };
} // ouchat::ai

#endif //AI_H
