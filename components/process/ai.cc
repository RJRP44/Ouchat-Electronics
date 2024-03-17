//
// Created by Romain on 09/03/2024.
//

#include <iostream>
#include "ai.h"

#include <esp_log.h>
#include <model.h>

using namespace ouchat;

namespace ouchat::ai
{

    uint8_t interpreter::tensor_arena[TENSOR_ARENA_SIZE]{};
    tflite::MicroInterpreter *interpreter::micro_interpreter;

    const char* tag = "ouchat_ai_interpreter";

    esp_err_t interpreter::init(const void* ai_model)
    {
        const tflite::Model* ouchat_model = tflite::GetModel(ai_model);

        if (ouchat_model->version() != TFLITE_SCHEMA_VERSION)
        {
            return ESP_FAIL;
        }

        //Add the fully connected layer
        static tflite::MicroMutableOpResolver<2> resolver;
        if (resolver.AddFullyConnected() != kTfLiteOk)
        {
            return ESP_FAIL;
        }

        if (resolver.AddSoftmax() != kTfLiteOk)
        {
            return ESP_FAIL;
        }

        // Build an interpreter to run the model with.
        interpreter::micro_interpreter = new tflite::MicroInterpreter(ouchat_model, resolver, tensor_arena, TENSOR_ARENA_SIZE);

        if (interpreter::micro_interpreter->AllocateTensors() != kTfLiteOk)
        {
            ESP_LOGE(tag, "Failed to allocate tensors");
            return ESP_FAIL;
        }

        ESP_LOGI(tag, "Oucha²i version : %f", OUCHA2I_VERSION);

        return ESP_OK;
    }

    esp_err_t interpreter::predict(model_input input, result* output)
    {

        memcpy(interpreter::micro_interpreter->input(0)->data.f, &input.to_normalized_array()[0], 23 * sizeof(float));

        // Run inference, and report any error
        TfLiteStatus invoke_status = interpreter::micro_interpreter->Invoke();
        if (invoke_status != kTfLiteOk) {
            ESP_LOGE(tag, "Invoke failed ...");
            return ESP_FAIL;
        }

        float outside = interpreter::micro_interpreter->output(0)->data.f[0];
        float inside = interpreter::micro_interpreter->output(0)->data.f[2];
        float other = interpreter::micro_interpreter->output(0)->data.f[1];

        std::cout << "Predictions : {" << outside << ";" << inside << ";" << other << "}" << std::endl;

        if (roundf(outside) == 1.0F && roundf(inside) == 0.0F && roundf(other) == 0.0F)
        {
            *output = OUTSIDE;

            return ESP_OK;
        }

        if (roundf(outside) == 0.0F && roundf(inside) == 1.0F && roundf(other) == 0.0F)
        {
            *output = INSIDE;

            return ESP_OK;
        }

        *output = OTHER;

        return ESP_OK;
    }

} // ouchat::ai
