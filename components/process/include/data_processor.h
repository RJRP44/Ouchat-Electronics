//
// Created by Romain on 28/01/2024.
//

#ifndef OUCHAT_ELECTRONICS_DATA_PROCESSOR_H
#define OUCHAT_ELECTRONICS_DATA_PROCESSOR_H

#include <esp_err.h>
#include <ouchat_types.h>

#define UNDEFINED (-1)
#define BACKGROUND (-2)

#define OBJ_CLEARANCE_MM 75

esp_err_t process_init();
esp_err_t print_frame(frame_t frame);
esp_err_t process_data(coord_t sensor_data[8][8], calibration_config_t calibration);

#endif //OUCHAT_ELECTRONICS_DATA_PROCESSOR_H
