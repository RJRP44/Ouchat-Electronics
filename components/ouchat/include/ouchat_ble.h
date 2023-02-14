//
// Created by Romain on 14/02/2023.
//

#ifndef OUCHAT_ELECTRONICS_OUCHAT_BLE_H
#define OUCHAT_ELECTRONICS_OUCHAT_BLE_H

#define DEVICE_NAME "Ouchat"
#define OUCHAT_BLE_TAG "Ouchat-BLE"
#define OUCHAT_APP_ID 0x23
#define SVC_INST_ID                 0

/* The max length of characteristic value. When the GATT client performs a write or prepare write operation,
*  the data length must be less than GATTS_CHAR_VAL_LEN_MAX.
*/
#define GATTS_CHAR_VAL_LEN_MAX 500


esp_err_t ouchat_ble_init();

enum {
    OCA_SVC,
    //Cat : the unique identifier of the car
    OCA_CHAR_CAT,
    OCA_CHAR_VAL_CAT,

    //SSID : the ssid of the ap, has to be set by the user/app,
    OCA_CHAR_SSID,
    OCA_CHAR_VAL_SSID,

    //Password : the password of the ap, has to be set by the user/app,
    OCA_CHAR_PSWD,
    OCA_CHAR_VAL_PSWD,

    //Wi-Fi status : the status of the wifi connection
    OCA_CHAR_WSTA,
    OCA_CHAR_VAL_WSTA,

    //APs : a list of all aps discovered
    OCA_CHAR_APS,
    OCA_CHAR_VAL_APS,
    OCA_CHAR_CFG_APS,

    HRS_OCA_NB,
};

#endif //OUCHAT_ELECTRONICS_OUCHAT_BLE_H
