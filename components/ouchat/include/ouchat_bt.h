//
// Created by RJRP on 23/10/2022.
//

#include <stdint.h>
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#ifndef OUCHAT_ELECTRONICS_OUCHAT_BL_H
#define OUCHAT_ELECTRONICS_OUCHAT_BL_H

static const char *SPP_TAG = "ouchat_bt";

#define SPP_SERVER_NAME "SPP_SERVER"
#define DEVICE_NAME "Ouchat"

static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

void ouchat_bt_init();

void ouchat_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
void ouchat_bt_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);

uint8_t ouchat_bt_cmd_interpreter(uint8_t *input, char **cmd_arg, char **cmd_value);

#endif //OUCHAT_ELECTRONICS_OUCHAT_BL_H
