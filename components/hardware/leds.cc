//
// Created by Romain on 25/02/2024.
//

#include <esp_err.h>
#include <types.h>
#include "leds.h"

led_strip_handle_t led_strip;

esp_err_t init_leds()
{
    //Init the LED strip config
    led_strip_config_t strip_config = {
        .strip_gpio_num = GPIO_NUM_48,
        .max_leds = 1,
    };

    //Init the RTM channel
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
        .flags = {.with_dma = false}
    };

    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    return ESP_OK;
}

esp_err_t set_color(color_t color)
{
    led_strip_set_pixel(led_strip, 0, color.red, color.green, color.blue);

    //Refresh the strip to send data
    return led_strip_refresh(led_strip);
}
