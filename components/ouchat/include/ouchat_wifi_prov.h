//
// Created by RJRP on 26/02/2023.
//

#include <stdbool.h>

#ifndef OUCHAT_ELECTRONICS_OUCHAT_WIFI_PROV_H
#define OUCHAT_ELECTRONICS_OUCHAT_WIFI_PROV_H

void ouchat_init_provisioning();
bool ouchat_is_provisioned();
void ouchat_start_provisioning();
void ouchat_deinit_provisioning();
void ouchat_get_sec2_salt(const char **salt, uint16_t *salt_len);
void ouchat_get_sec2_verifier(const char **verifier, uint16_t *verifier_len);
void get_device_name(char *service_name, size_t max);

#endif //OUCHAT_ELECTRONICS_OUCHAT_WIFI_PROV_H
