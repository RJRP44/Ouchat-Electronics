//
// Created by Romain on 28/01/2024.
//

#ifndef OUCHAT_ELECTRONICS_WIFI_H
#define OUCHAT_ELECTRONICS_WIFI_H

#include <esp_err.h>

#define WIFI_CONNECTED BIT0
#define WIFI_FAIL BIT1
#define MAXIMUM_RETRY 4
#define THREADED_TASK NULL

void wifi_init(void *value);

#endif //OUCHAT_ELECTRONICS_WIFI_H
