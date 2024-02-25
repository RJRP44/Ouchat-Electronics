//
// Created by Romain on 25/02/2024.
//

#ifndef OUCHAT_ELECTRONICS_LEDS_H
#define OUCHAT_ELECTRONICS_LEDS_H

#include <led_strip.h>

extern led_strip_handle_t led_strip;

esp_err_t init_leds();
esp_err_t set_color(color_t color);

#endif //OUCHAT_ELECTRONICS_LEDS_H
