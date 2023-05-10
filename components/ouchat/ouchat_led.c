//
// Created by RJRP on 18/03/2023.
//

#include "include/ouchat_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ouchat_animate(led_strip_handle_t led_strip, enum animation_type animation, enum animation_direction direction, uint16_t delay, rgb_color* from,
                    rgb_color to) {
    if (animation == SLIDE) {
        for (int i = 0; i < 13; ++i) {
            for (int j = 0; j < 256; ++j) {

                led_strip_set_pixel(led_strip, direction == RIGHT ? i : (12 - i),
                                    j * (to.red - (*from).red) / 255 + (*from).red,
                                    j * (to.green - (*from).green) / 255 + (*from).green,
                                    j * (to.blue - (*from).blue) / 255 + (*from).blue);

                if (j == 255)
                    led_strip_set_pixel(led_strip, direction == RIGHT ? i : (12 - i), to.red, to.green, to.blue);

                led_strip_refresh(led_strip);
                vTaskDelay(pdMS_TO_TICKS(delay / (255))/13);
            }
        }
    } else if (animation == FADE) {
        for (int j = 0; j < 256; ++j) {
            for (int i = 0; i < 13; ++i) {

                led_strip_set_pixel(led_strip, i,
                                    j * (to.red - (*from).red) / 255 + (*from).red,
                                    j * (to.green - (*from).green) / 255 + (*from).green,
                                    j * (to.blue - (*from).blue) / 255 + (*from).blue);

                if (j == 255)
                    led_strip_set_pixel(led_strip, i, to.red, to.green, to.blue);
            }
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(delay / 255));
        }
    }

    *from = to;
}

void ouchat_error(led_strip_handle_t led_strip, uint16_t delay, rgb_color *from, rgb_color to) {
    for (int j = 0; j < 256; ++j) {
        for (int i = 13; i < 19; ++i) {

            led_strip_set_pixel(led_strip, i,
                                j * (to.red - (*from).red) / 255 + (*from).red,
                                j * (to.green - (*from).green) / 255 + (*from).green,
                                j * (to.blue - (*from).blue) / 255 + (*from).blue);

            if (j == 255)
                led_strip_set_pixel(led_strip, i, to.red, to.green, to.blue);
        }
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(delay / 255));
    }

    *from = to;
}

bool sameColor(rgb_color a, rgb_color b){
    if (a.blue != b.blue){
        return false;
    }
    if (a.red != b.red){
        return false;
    }
    if (a.green != b.green){
        return false;
    }
    return true;
}