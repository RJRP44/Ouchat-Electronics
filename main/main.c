#include <sys/cdefs.h>
#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "vl53l5cx_api.h"
#include "ouchat_api.h"

static const char *TAG = "v53l5cx_lib";

#define I2C_SDA_NUM                      21
#define I2C_SCL_NUM                      22
#define I2C_CLK_SPEED                    1000000
#define I2C_TIMEOUT                      400000
#define I2C_TX_BUF                       0
#define I2C_RX_BUF                       0

static esp_err_t i2c_master_init(void) {

    i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_SDA_NUM,
            .scl_io_num = I2C_SCL_NUM,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_CLK_SPEED,
    };

    i2c_param_config(I2C_NUM_1, &conf);
    i2c_set_timeout(I2C_NUM_1, I2C_TIMEOUT);

    return i2c_driver_install(I2C_NUM_1, conf.mode, I2C_RX_BUF, I2C_TX_BUF, 0);
}

_Noreturn void app_main(void) {

    //Initialize the i2c bus
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    uint8_t status, isAlive, i , isReady;

    //Define the i2c address and port
    VL53L5CX_Configuration *configuration = malloc(sizeof *configuration);
    configuration->platform.port = I2C_NUM_1;
    configuration->platform.address = 0x52;

    //Wakeup the sensor
    status = vl53l5cx_is_alive(configuration, &isAlive);
    if (!isAlive || status) {
        printf("VL53L5CX not detected at requested address\n");
        while (1);
    }

    /* (Mandatory) Init VL53L5CX sensor */
    status = vl53l5cx_init(configuration);
    if (status) {
        printf("VL53L5CX ULD Loading failed\n");
        while (1);
    }

    printf("VL53L5CX ULD ready ! (Version : %s)\n", VL53L5CX_API_REVISION);

    status = vl53l5cx_set_resolution(configuration, VL53L5CX_RESOLUTION_8X8);
    if (status) {
        printf("vl53l5cx_set_resolution failed, status %u\n", status);
        while (1);
    }

    status = vl53l5cx_set_ranging_frequency_hz(configuration, 15);
    if(status)
    {
        printf("vl53l5cx_set_ranging_frequency_hz failed, status %u\n", status);
        while (1);
    }

    uint8_t resolution;
    vl53l5cx_get_resolution(configuration,&resolution);
    printf("resolution : %d \n", (uint8_t) sqrt(resolution));
    status = vl53l5cx_start_ranging(configuration);

    VL53L5CX_ResultsData results;
    uint8_t output[64];
    uint8_t *p_output = &output[0];
    int16_t background[64];
    uint8_t hasback = 0;

    while (1) {

        /* Use polling function to know when a new measurement is ready.
         * Another way can be to wait for HW interrupt raised on PIN A3
         * (GPIO 1) when a new measurement is ready */

        memset(output,0,64);
        status = vl53l5cx_check_data_ready(configuration, &isReady);
        if (isReady) {
            vl53l5cx_get_ranging_data(configuration, &results);
            if(hasback == 0){
                hasback = 1;
                for (int j = 0; j < 64; ++j) {
                    background[j] = results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE*j];
                    printf("v = %4d\n",results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE*j]);
                }
            }else{
                ouchat_process_data(results.distance_mm,background,p_output);
                for (int j = 0; j < 64; ++j) {
                    if (j % 8 == 0) {
                        printf("\n");
                    }
                    if(output[j] == 0){
                        printf("  ");
                    }else{
                        printf("%d ",output[j]);
                    }

                }
            }
            printf("\n");
        }

        /* Wait a few ms to avoid too high polling (function in platform
         * file, not in API) */
        WaitMs(&(configuration->platform), 5);
    }

    /*
     * while (true) {

        * Use polling function to know when a new measurement is ready.
         * Another way can be to wait for HW interrupt raised on PIN A3
         * (GPIO 1) when a new measurement is ready *

    status = vl53l5cx_check_data_ready(configuration, &isReady);
    printf("status %d\n", status);
    if (isReady) {
        vl53l5cx_get_ranging_data(configuration, &results);
        printf("Ranfdsfdsffds");
        if(hasback == 0){
            hasback = 1;
            for (int j = 0; j < 64; ++j) {
                background[j] = results.distance_mm[j];
                printf("v = %d\n",results.distance_mm[j]);
            }
        }else{
            ouchat_process_data(&results,background,output);
            for (int j = 0; j < 64; ++j) {
                printf("%d,",output[j]);
            }
            printf("\n");
        }

    }

     Wait a few ms to avoid too high polling (function in platform
     * file, not in API)
    WaitMs(&(configuration->platform), 5);
}
     */

}