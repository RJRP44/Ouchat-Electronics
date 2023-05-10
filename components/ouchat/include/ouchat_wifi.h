//
// Created by Romain on 26/02/2023.
//

#include "freertos/event_groups.h"
#include "ouchat_led.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_WIFI_H
#define OUCHAT_ELECTRONICS_OUCHAT_WIFI_H

void ouchat_wifi_register_handlers();
void ouchat_wifi_register_events(led_strip_handle_t *led_strip);

#endif //OUCHAT_ELECTRONICS_OUCHAT_WIFI_H
