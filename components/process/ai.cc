//
// Created by Romain on 09/03/2024.
//

#include <iostream>
#include "ai.h"
#include <esp_log.h>
#include <model.h>
#include "api.h"
#include "leds.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

#if CONFIG_OUCHAT_DEBUG_LOGGER
#include "mbedtls/base64.h"
#endif

using namespace ouchat;

namespace ouchat::ai
{
    bool initialized = false;
    uint8_t tensor_arena[TENSOR_ARENA_SIZE]{};
    tflite::MicroInterpreter *micro_interpreter = nullptr;

    QueueHandle_t input_queue = nullptr;
    TaskHandle_t task_handle = nullptr;


    result interpreter::classify(const TfLitePtrUnion& data)
    {
        const float outside = data.f[0];
        const float inside = data.f[2];
        const float other = data.f[1];

        if (roundf(outside) == 1.0F && roundf(inside) == 0.0F && roundf(other) == 0.0F)
        {
            return OUTSIDE;
        }

        if (roundf(outside) == 0.0F && roundf(inside) == 1.0F && roundf(other) == 0.0F)
        {
            return INSIDE;
        }

        return OTHER;
    }

    esp_err_t interpreter::init(const void* ai_model)
    {
        if (initialized)
        {
            return ESP_OK;
        }

        input_queue = xQueueCreate(AI_QUEUE_DEPTH, sizeof(model_input));
        if (input_queue == nullptr)
        {
            ESP_LOGE(AI_LOG_TAG, "Failed to create AI input queue");
            return ESP_ERR_NO_MEM;
        }

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
        static tflite::MicroInterpreter static_interpreter(ouchat_model, resolver, tensor_arena, TENSOR_ARENA_SIZE);
        micro_interpreter = &static_interpreter;

        if (micro_interpreter->AllocateTensors() != kTfLiteOk)
        {
            ESP_LOGE(AI_LOG_TAG, "Failed to allocate tensors");
            return ESP_FAIL;
        }

        ESP_LOGI(AI_LOG_TAG, "Oucha²i version : v%.01f", OUCHA2I_VERSION);
        initialized = true;

        const auto status = xTaskCreatePinnedToCore(task_loop, "oucha2i", 8192, nullptr, 4, &task_handle, 1);
        if (status != pdPASS)
        {
            ESP_LOGE(AI_LOG_TAG, "Failed to create AI task");
            vQueueDelete(input_queue);
            input_queue = nullptr;
            return ESP_FAIL;
        }

        return ESP_OK;
    }

    esp_err_t interpreter::predict(model_input input, result& output)
    {

        if (!initialized)
        {
            ESP_LOGE(AI_LOG_TAG, "Model is not initialized");

            if (init(model) != ESP_OK)
            {
                return ESP_FAIL;
            }
        }

        memcpy(micro_interpreter->input(0)->data.f, &input.to_normalized_array()[0], 23 * sizeof(float));

        // Run inference, and report any error
        TfLiteStatus invoke_status = micro_interpreter->Invoke();
        if (invoke_status != kTfLiteOk) {
            ESP_LOGE(AI_LOG_TAG, "Invoke failed ...");
            return ESP_FAIL;
        }

        float outside = micro_interpreter->output(0)->data.f[0];
        float inside = micro_interpreter->output(0)->data.f[2];
        float other = micro_interpreter->output(0)->data.f[1];

        //For debug purpose
        ESP_LOGI(AI_LOG_TAG, "Prediction {%f,%f,%f}", outside, inside, other);

        output = classify(micro_interpreter->output(0)->data);

        return ESP_OK;
    }

    void interpreter::task_loop(void*)
    {
        model_input input{};
        for (;;)
        {
            if (xQueueReceive(input_queue, &input, portMAX_DELAY) != pdTRUE)
            {
                vTaskDelay(5 / portTICK_PERIOD_MS);
                continue;
            }

            if (!initialized)
            {
                ESP_LOGE(AI_LOG_TAG, "Model is not initialized");
                continue;
            }

            result results;
            if (predict(input, results) == ESP_OK)
            {

#if CONFIG_OUCHAT_DEBUG_LOGGER
                //Print the debug data

                size_t length;
                std::string log;
                log.resize(130);

                mbedtls_base64_encode(reinterpret_cast<unsigned char*>(log.data()), log.size(), &length, reinterpret_cast<const unsigned char*>(&input),sizeof (input));

                ESP_LOGI(AI_LOG_TAG, "%s", log.c_str());
#endif

                TaskHandle_t xHandle = nullptr;

                if (results == OUTSIDE)
                {
                    ESP_LOGI(AI_LOG_TAG, "Outside !");

                    //Set the LED color
                    set_color({.red = 50, .green = 0, .blue = 0});

                    int16_t value = 0;

                    xTaskCreatePinnedToCore(api_set, "ouchat_api", 4096, &value, 4, &xHandle, 1);
                    configASSERT(xHandle);
                }

                if (results == INSIDE)
                {
                    ESP_LOGI(AI_LOG_TAG, "Inside !");

                    //Set the LED color
                    set_color({.red = 0, .green = 50, .blue = 0});

                    int16_t value = 1;

                    xTaskCreatePinnedToCore(api_set, "ouchat_api", 4096, &value, 4, &xHandle, 1);
                    configASSERT(xHandle);
                }
            }
        }
    }

    esp_err_t interpreter::submit(const model_input& input, TickType_t wait_ticks)
    {
        if (input_queue == nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }
        return xQueueSend(input_queue, &input, wait_ticks) == pdTRUE ? ESP_OK : ESP_FAIL;
    }

} // ouchat::ai
