//
// Created by Romain on 25/02/2024.
//

#ifndef OUCHAT_ELECTRONICS_API_H
#define OUCHAT_ELECTRONICS_API_H

#define API_TAG "api"

#define REQUEST_FLAG BIT1
#define REQUEST_DONE_FLAG BIT2

extern EventGroupHandle_t api_flags;

esp_err_t init_api();
void api_set(void *args);

#endif //OUCHAT_ELECTRONICS_API_H
