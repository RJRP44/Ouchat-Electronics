//
// Created by RJRP on 19/06/2023.
//

#include "stdint.h"
#include "stddef.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_LOGGER_H
#define OUCHAT_ELECTRONICS_OUCHAT_LOGGER_H

uint8_t ouchat_logger_init();
uint8_t ouchat_log(unsigned char *data);
void ouchat_logger(void *arg);
uint8_t ouchat_deep_sleep_logger();
#endif //OUCHAT_ELECTRONICS_OUCHAT_LOGGER_H
