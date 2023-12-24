//
// Created by RJRP on 19/06/2023.
//

#include "stdint.h"
#include "stddef.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_LOGGER_H
#define OUCHAT_ELECTRONICS_OUCHAT_LOGGER_H

#define OUCHAT_LOG_EVENT_STOPPED BIT0
#define OUCHAT_LOG_SIZE 190
#define OUCHAT_LOG_QUEUE_SIZE 200

uint8_t ouchat_logger_init();
uint8_t ouchat_log(unsigned char *data, size_t size);
void ouchat_logger(void *arg);
uint8_t ouchat_stop_logger();
#endif //OUCHAT_ELECTRONICS_OUCHAT_LOGGER_H
