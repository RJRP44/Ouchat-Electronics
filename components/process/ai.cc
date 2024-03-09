//
// Created by Romain on 09/03/2024.
//

#include <iostream>
#include "ai.h"

#include <esp_log.h>

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
        static tflite::MicroMutableOpResolver<1> resolver;
        if (resolver.AddFullyConnected() != kTfLiteOk)
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

        float result = utils::denormalize(interpreter::micro_interpreter->output(0)->data.f[0],-1,1);
        ESP_LOGI(tag, "Prediction : %f",result);

        if (abs(result) > 1.5){
            *output = OTHER;
        }

        *output = ai::result(roundf(result));

        return ESP_OK;
    }

} // ouchat::ai
