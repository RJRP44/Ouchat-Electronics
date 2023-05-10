//
// Created by RJRP on 18/03/2023.
//
#include "led_strip.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_LED_H
#define OUCHAT_ELECTRONICS_OUCHAT_LED_H

#define OUCHAT_LED_STRIP_GPIO 21
#define OUCHAT_LED_STRIP_LENGTH 18

#define DEFAULT_OUCHAT_LED_STRIP_CONFIG {\
.strip_gpio_num = OUCHAT_LED_STRIP_GPIO,\
.max_leds = OUCHAT_LED_STRIP_LENGTH,\
.led_pixel_format = LED_PIXEL_FORMAT_GRB,\
.led_model = LED_MODEL_WS2812,\
.flags.invert_out = false,\
}

#define DEFAULT_OUCHAT_RMT_CONFIG {\
.clk_src = RMT_CLK_SRC_DEFAULT,\
.resolution_hz = 10 * 1000 * 1000,\
.flags.with_dma = false,\
}
typedef struct {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} rgb_color;

enum animation_type {
    SLIDE,
    FADE
};

enum animation_direction {
    RIGHT,
    LEFT
};

void ouchat_animate(led_strip_handle_t led_strip, enum animation_type animation, enum animation_direction direction, uint16_t delay, rgb_color* from, rgb_color to);
void ouchat_error(led_strip_handle_t led_strip, uint16_t delay, rgb_color* from, rgb_color to);
bool sameColor(rgb_color a, rgb_color b);

#endif //OUCHAT_ELECTRONICS_OUCHAT_LED_H
