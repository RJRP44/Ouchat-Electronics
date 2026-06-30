//
// Created by Romain on 19/06/2023.
//

#ifndef OUCHAT_ELECTRONICS_LOGGER_H
#define OUCHAT_ELECTRONICS_LOGGER_H

#include <string>

#define OUCHAT_LOG_EVENT_STOPPED BIT0
#define OUCHAT_LOG_SIZE 500
#define OUCHAT_RAW_DATA_SIZE 1825
#define OUCHAT_RAW_PACKET_SIZE 1950
#define OUCHAT_LOG_QUEUE_SIZE 700

uint8_t tcp_logger_set_timecode(const uint8_t *timecode);
uint8_t init_tcp_logger();

uint8_t tcp_log(uint32_t timestamp, const std::string& data);
uint8_t stop_tcp_logger();
void tcp_logger_task(void *arg);

#endif //OUCHAT_ELECTRONICS_LOGGER_H
