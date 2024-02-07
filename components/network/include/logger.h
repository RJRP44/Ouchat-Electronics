//
// Created by Romain on 19/06/2023.
//

#ifndef OUCHAT_ELECTRONICS_LOGGER_H
#define OUCHAT_ELECTRONICS_LOGGER_H

#define OUCHAT_LOG_EVENT_STOPPED BIT0
#define OUCHAT_LOG_SIZE 190
#define OUCHAT_LOG_QUEUE_SIZE 200

uint8_t init_tcp_logger();
uint8_t tcp_log(unsigned char *data);
uint8_t stop_tcp_logger();
void tcp_logger_task(void *arg);

#endif //OUCHAT_ELECTRONICS_LOGGER_H
