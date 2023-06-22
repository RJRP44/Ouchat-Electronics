//
// Created by RJRP on 29/06/2022.
//

#include "stdint.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_API_H
#define OUCHAT_ELECTRONICS_OUCHAT_API_H

#define REQUESTING_API 1
#define WAITING_WIFI 2

extern uint8_t ouchat_api_status;

void ouchat_api_set(void *pvparameters);
void ouchat_api_join(void *token);

#endif //OUCHAT_ELECTRONICS_OUCHAT_API_H
