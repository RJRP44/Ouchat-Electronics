#include <sys/cdefs.h>
#include <stdio.h>
#include <math.h>
#include <esp_timer.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "sdkconfig.h"
#include "vl53l8cx_api.h"
#include "ouchat_api.h"
#include <string.h>
#include <cJSON.h>
#include <esp_sleep.h>
#include <mbedtls/base64.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "ouchat_wifi.h"
#include "ouchat_led.h"
#include "ouchat_sensor.h"
#include "ouchat_logger.h"
#include "vl53l8cx_plugin_detection_thresholds.h"
#include "ouchat_calibrator.h"
#include "dbscan.h"
#include "ouchat_utils.h"

static const char *TAG = "Ouchat-Main";

RTC_DATA_ATTR VL53L8CX_Configuration ouchat_sensor_configuration;
RTC_DATA_ATTR int16_t ouchat_sensor_context = 0;
RTC_DATA_ATTR int16_t ouchat_sensor_background[64];
RTC_DATA_ATTR ouchat_calibration_config calibration;

#define MIN_SIZE 1
#define MAX_CLUSTERS (64 / (MIN_SIZE + 1))

#define PREVIOUS_FRAME 0
#define MIX_PREVIOUS_FRAME 1
#define CURRENT_FRAME 2
#define MIX_CURRENT_FRAME 3

#define PREVIOUS 0
#define CURRENT 1
#define MIX 2
#define TEMP_CURRENT 3

cluster_points_t previous_points;

cluster_points_t clusters_points[4];
cluster_t clusters[2][MAX_CLUSTERS];


ouchat_motion_threshold_config threshold_config = {
        .distance_min = 400,
        .distance_max = 800,
        .motion_threshold = 44
};

static void ouchat_event_handler(double_t x, double_t y, area_t start, area_t end) {
/*
    if (end.left_center.y == 0 && start.center.y >= 4.5) {

        printf("Fast Outside\n");

        ouchat_api_status = REQUESTING_API;

        TaskHandle_t xHandle = NULL;

        xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 4, &xHandle);
        configASSERT(xHandle);
    } else if (y >= 4.5) {
        if (end.center.y <= 2) {

            printf("Outside\n");

            ouchat_api_status = REQUESTING_API;
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 0, 4, &xHandle);
            configASSERT(xHandle);

        } else {
            printf("Fake Outside\n");
        }
    } else if (y <= -4.5) {
        if (start.center.y <= 2) {

            printf("Inside\n");

            ouchat_api_status = REQUESTING_API;
            TaskHandle_t xHandle = NULL;

            xTaskCreate(ouchat_api_set, "ouchat_api_set", 8192, (void *) 1, 4, &xHandle);
            configASSERT(xHandle);
        } else {
            printf("Fake Inside\n");
        }
    }*/
}

void print_frame(int8_t frame[8][8], uint8_t frame_id) {

    printf("---------------- %d\n", frame_id);
    for (X_Y_FOR_LOOP) {
        if(frame[x][y] >= 0){
            printf("\e[4%im %i", frame[x][y] + 1, frame[x][y]);
        }else{
            printf("\e[4%im  ", 0);
        }

        if (y == 7) {
            printf("\e[40m\n");
        }
    }
}

int cluster_size_comparator(const void *first, const void *second) {
    uint8_t first_cluster_id = *(const uint8_t *) first;
    uint8_t second_cluster_id = *(const uint8_t *) second;

    //Make sure the data is in the table
    if (first_cluster_id >= clusters_points[PREVIOUS].clusters || second_cluster_id >= clusters_points[PREVIOUS].clusters) {
        return 1;
    }

    return clusters[PREVIOUS][second_cluster_id].size - clusters[PREVIOUS][first_cluster_id].size;
}

void app_main(void) {

    ouchat_api_status = WAITING_WIFI;

    //Get the wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();

    //Define the i2c bus configuration
    i2c_config_t i2c_config = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = OUCHAT_SENSOR_DEFAULT_SDA,
            .scl_io_num = OUCHAT_SENSOR_DEFAULT_SCL,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_DEFAULT_CLK_SPEED,
    };

    //Apply, init the configuration to the bus
    ouchat_init_i2c(I2C_NUM_1, i2c_config);

    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {

        esp_err_t nvs_ret = nvs_flash_init();

        if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

            nvs_flash_erase();
            nvs_flash_init();
        }

        esp_netif_init();

        esp_event_loop_create_default();

        ouchat_wifi_register_events();

        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        //Power on sensor and init
        ouchat_sensor_configuration.platform.address = VL53L8CX_DEFAULT_I2C_ADDRESS;
        ouchat_sensor_configuration.platform.port = I2C_NUM_1;

        ouchat_sensor_config sensor_config = {
                .sensor_config = &ouchat_sensor_configuration,
                .ranging_mode = VL53L8CX_RANGING_MODE_CONTINUOUS,
                .frequency_hz = 15,
                .resolution = VL53L8CX_RESOLUTION_8X8
        };

        ouchat_init_sensor(sensor_config);

        //Get the context / environment of the scan
        vl53l8cx_start_ranging(&ouchat_sensor_configuration);

        uint8_t is_ready = 0;
        vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);

        //Wait for the sensor to start ranging
        while (!is_ready) {
            WaitMs(&(ouchat_sensor_configuration.platform), 5);
            vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
        }


        previous_points.count = 0;
        previous_points.clusters = 0;

        //Calibrate the sensor to this environment
        printf("Starting sensor calibration\n");
        ouchat_calibrate_sensor(&calibration, &ouchat_sensor_configuration);
        printf("Sensor angles determined by %d points :\n X : %f\n Z : %f\n", calibration.inliers, (PI - calibration.angles.z) * 180 / PI,(PI / 2 - calibration.angles.x) * 180 / PI);

/*
        //Get the raw data
        VL53L8CX_ResultsData results;
        vl53l8cx_get_ranging_data(&ouchat_sensor_configuration, &results);

        //Process it the get the approximate distance
        ouchat_get_context(results.distance_mm, &ouchat_sensor_context);

        memcpy(ouchat_sensor_background, results.distance_mm, sizeof results.distance_mm);

        printf("context distance : %i mm \n", ouchat_sensor_context);

        for (int j = 0; j < 8; ++j) {
            for (int k = 0; k < 8; ++k) {
                printf("%i ", results.distance_mm[8 * j + k]);
            }
            printf("\n");
        }*/

        //Stop the ranging to set the sensor in "sleep"
        vl53l8cx_stop_ranging(&ouchat_sensor_configuration);

        sensor_config = (ouchat_sensor_config) {
                .sensor_config = &ouchat_sensor_configuration,
                .ranging_mode = VL53L8CX_RANGING_MODE_AUTONOMOUS,
                .frequency_hz = 5,
                .resolution = VL53L8CX_RESOLUTION_8X8,
                .integration_time = 10,
        };

        ouchat_lp_sensor(sensor_config, threshold_config, &ouchat_sensor_background[0]);
        vl53l8cx_start_ranging(&ouchat_sensor_configuration);

        esp_deep_sleep_disable_rom_logging();

        esp_sleep_enable_ext0_wakeup(GPIO_NUM_3, 0);
        esp_deep_sleep_start();
    }

    //By using Interrupt autostop, the frame measurements are aborted when an interrupt is triggered
    ouchat_sensor_config sensor_config = (ouchat_sensor_config) {
            .sensor_config = &ouchat_sensor_configuration,
            .ranging_mode = VL53L8CX_RANGING_MODE_AUTONOMOUS,
            .frequency_hz = 5,
            .resolution = VL53L8CX_RESOLUTION_8X8,
            .integration_time = 10,
    };

    vl53l8cx_stop_ranging(&ouchat_sensor_configuration);

    vl53l8cx_set_ranging_mode(&ouchat_sensor_configuration, VL53L8CX_RANGING_MODE_CONTINUOUS);
    vl53l8cx_set_ranging_frequency_hz(&ouchat_sensor_configuration, 15);
    vl53l8cx_set_detection_thresholds_auto_stop(&ouchat_sensor_configuration, 0);
    vl53l8cx_start_ranging(&ouchat_sensor_configuration);

    //Get the raw data
    VL53L8CX_ResultsData results;
    uint8_t is_ready = 0;
    uint8_t status;

    //Wait for the sensor to restart ranging
    while (!is_ready) {
        WaitMs(&(ouchat_sensor_configuration.platform), 5);
        vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);
    }

    //ouchat_processing_wakeup();


#if CONFIG_OUCHAT_DEBUG_LOGGER

    ouchat_logger_init();

    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(ouchat_logger, "ouchat_log", 16384, &ouchat_sensor_context, 5, &xHandle, 1);
    configASSERT(xHandle);
#else

    TaskHandle_t xHandle = NULL;

    xTaskCreatePinnedToCore(ouchat_wifi_wakeup, "ouchat_wifi", 16384,NULL, 5, &xHandle, 1);
    configASSERT(xHandle);

#endif


    uint16_t empty_frames = 0;

    while (empty_frames < 30) {
        status = vl53l8cx_check_data_ready(&ouchat_sensor_configuration, &is_ready);

        if (is_ready) {
            vl53l8cx_get_ranging_data(&ouchat_sensor_configuration, &results);
            //status = ouchat_handle_data(results.distance_mm,ouchat_sensor_context,&ouchat_event_handler);
            coordinates_t sensor_data[8][8];
            uint16_t raw_data[8][8];
            memcpy(raw_data, results.distance_mm, sizeof(raw_data));

            ouchat_correct_data(calibration, raw_data, sensor_data);

            bool background_mask[8][8] = {0};
            uint8_t background_point_count = 0;

            //Create 4 frame for current, previous and mixed data
            int8_t frames[4][8][8];
            memset(frames, -1, sizeof(frames));

            //Create a mask for the background
            for (X_Y_FOR_LOOP) {
                //All point near the floor (±150mm)

                if (fabs(sensor_data[x][y].z - calibration.floor_distance) < 150) {
                    background_mask[x][y] = true;
                    ++background_point_count;
                }

            }

            //Form groups of point for objects using DBSCAN
            double epsilon = PI / 3;
            clusters_points[CURRENT].count = 64 - background_point_count;
            clusters_points[CURRENT].points = malloc(clusters_points[CURRENT].count * sizeof(point_t));
            clusters_points[CURRENT].clusters = 0;
            clusters_points[PREVIOUS].count = previous_points.count;
            clusters_points[PREVIOUS].clusters = previous_points.clusters;

            //Add all non-background point to be proceeded
            uint8_t i = 0;
            for (X_Y_FOR_LOOP) {
                if (!background_mask[x][y]) {

                    point_t *point = &clusters_points[CURRENT].points[i];

                    //Init the points and set there custer and frame id
                    memcpy(&point->coordinates, &sensor_data[x][y], sizeof(coordinates_t));
                    point->pixel = (pixel_coordinates_t) {x, y};
                    point->cluster_id = UNCLASSIFIED;
                    point->frame_id = CURRENT_FRAME;
                    point->measurement = raw_data[x][y];
                    ++i;
                }
            }

            //Use DBSCAN algorithm to group points in clusters
            dbscan(clusters_points[CURRENT], epsilon, MIN_SIZE, euclidean_dist);

            //Set the point's cluster id on the frame for future comparisons
            for (uint8_t j = 0; j < clusters_points[CURRENT].count; ++j) {

                point_t *point = &clusters_points[CURRENT].points[j];
                pixel_coordinates_t *pixel = &point->pixel;

                frames[CURRENT_FRAME][pixel->x][pixel->y] = point->cluster_id;

                //The biggest cluster id equals the cluster count
                clusters_points[CURRENT].clusters = MAX(clusters_points[CURRENT].clusters, point->cluster_id + 1);
            }


            //Print the clusters id
            //print_frame(frames[CURRENT_FRAME], line);

            //Add 2 frames together to track the displacement only if the previous frame contains data
            clusters_points[MIX].count = clusters_points[PREVIOUS].count + clusters_points[CURRENT].count;
            clusters_points[MIX].points = malloc(clusters_points[MIX].count * sizeof(point_t));
            clusters_points[MIX].clusters = 0;

            clusters_points[PREVIOUS].points = previous_points.points;

            //Concatenate the points of previous and current data
            memcpy(clusters_points[MIX].points, clusters_points[CURRENT].points,
                   clusters_points[CURRENT].count * sizeof(point_t));

            //Add previous points after the current points
            point_t *current_points_end = &clusters_points[MIX].points[clusters_points[CURRENT].count];
            size_t previous_size = clusters_points[PREVIOUS].count * sizeof(point_t);
            memcpy(current_points_end, clusters_points[PREVIOUS].points, previous_size);

            //Init the points and reset there custer and frame id
            for (uint8_t j = 0; j < clusters_points[MIX].count; ++j) {
                point_t *point = &clusters_points[MIX].points[j];
                point->cluster_id = UNCLASSIFIED;

                //Set the corresponding frame id but mixed
                point->frame_id++;
            }

            //Use DBSCAN algorithm to group mix points in clusters
            dbscan(clusters_points[MIX], epsilon, MIN_SIZE, euclidean_dist);

            //Find between the 3 the maximum number of clusters
            int8_t max_cluster = MAX(clusters_points[MIX].clusters, clusters_points[CURRENT].clusters);
            max_cluster = MAX(max_cluster, clusters_points[PREVIOUS].clusters);

            //Create an equivalence table to follow a cluster even if its id changes
            uint8_t poll[2][max_cluster][max_cluster];
            int8_t equivalence_table[max_cluster];
            int8_t temp_table[2][max_cluster];
            uint8_t processing_order[max_cluster];

            memset(poll, 0, sizeof(poll));
            memset(equivalence_table, -1, sizeof(equivalence_table));
            memset(temp_table, -1, sizeof(temp_table));
            memset(clusters[CURRENT], 0, sizeof(clusters[CURRENT]));

            if (previous_points.count == 0) {
                goto end;
            }

            //Previous frame
            for (uint8_t j = 0; j < clusters_points[PREVIOUS].count; ++j) {
                pixel_coordinates_t pixel = clusters_points[PREVIOUS].points[j].pixel;
                frames[PREVIOUS_FRAME][pixel.x][pixel.y] = clusters_points[PREVIOUS].points[j].cluster_id;
            }

            //Previous mix frame
            for (uint8_t j = 0; j < clusters_points[MIX].count; ++j) {
                point_t *point = &clusters_points[MIX].points[j];
                pixel_coordinates_t pixel = point->pixel;
                frames[point->frame_id][pixel.x][pixel.y] = point->cluster_id;
            }
            /*
            print_frame(frames[CURRENT_FRAME], line ,CURRENT_FRAME);
            print_frame(frames[MIX_CURRENT_FRAME], line ,MIX_CURRENT_FRAME);
            print_frame(frames[MIX_PREVIOUS_FRAME], line ,MIX_PREVIOUS_FRAME);
            print_frame(frames[PREVIOUS_FRAME], line ,PREVIOUS_FRAME);*/

            for (uint8_t j = 0; j < 2; ++j) {
                for (X_Y_FOR_LOOP) {
                    int8_t past = frames[2 * j][x][y];
                    int8_t past_mix = frames[2 * j + 1][x][y];

                    if (past < 0 || past_mix < 0)
                        continue;

                    poll[j][past][past_mix] = poll[j][past][past_mix] + 1;
                }

                for (int8_t past = 0; past < max_cluster; ++past) {
                    uint8_t max = 0;
                    int8_t cluster = -1;
                    for (int8_t past_mix = 0; past_mix < max_cluster; ++past_mix) {
                        if (max < poll[j][past][past_mix]) {
                            max = poll[j][past][past_mix];
                            cluster = past_mix;
                        }
                    }
                    if (max != 0) {
                        temp_table[j][past] = cluster;
                    }
                }
            }

            /*
            //Search new clusters
            for (int8_t j = 0; j < max_cluster; ++j) {
                bool used = 0;
                for (int k = 0; k < max_cluster && !used; ++k) {
                    used = temp_table[CURRENT][j] == temp_table[PREVIOUS][k];
                }
                if (!used && temp_table[CURRENT][j] != -1){
                    //New cluster, add it to the table where none is defined
                    for (int k = 0; k < max_cluster; ++k) {
                        if (temp_table[PREVIOUS][k] == -1){
                            temp_table[PREVIOUS][k] = j;
                        }
                    }
                    printf("%d cluster is new !!\n",j);
                }
                if (!used && temp_table[CURRENT][j] == -1){
                    //End of a cluster
                    printf("%d cluster is dead ...\n",j );
                }
            }*/

            for (uint8_t j = 0; j < max_cluster; ++j) {
                processing_order[j] = j;
            }

            qsort(processing_order, max_cluster, sizeof(uint8_t), cluster_size_comparator);

            for (int8_t j = 0; j < max_cluster; ++j) {
                uint8_t previous = processing_order[j];

                for (int8_t k = 0; k < max_cluster; ++k) {
                    uint8_t current = processing_order[k];

                    if (temp_table[PREVIOUS][previous] == temp_table[CURRENT][current] &&
                        equivalence_table[current] == -1) {
                        printf("%d -> %d\n", current, previous);
                        equivalence_table[current] = (int8_t) previous;
                    }
                }
            }

            //Save current into the temp one
            memcpy(&clusters_points[TEMP_CURRENT], &clusters_points[CURRENT], sizeof(cluster_points_t));

            //Copy all points
            clusters_points[TEMP_CURRENT].points = malloc(clusters_points[CURRENT].count * sizeof(point_t));
            memcpy(clusters_points[TEMP_CURRENT].points, clusters_points[CURRENT].points,
                   clusters_points[CURRENT].count * sizeof(point_t));

            //Make changes
            for (uint8_t j = 0; j < max_cluster; ++j) {
                uint8_t cluster = processing_order[j];

                if (equivalence_table[cluster] == -1)
                    continue;

                for (int k = 0; k < clusters_points[CURRENT].count; ++k) {
                    point_t *point = &clusters_points[TEMP_CURRENT].points[k];
                    if (point->cluster_id == cluster) {
                        clusters_points[CURRENT].points[k].cluster_id = equivalence_table[cluster];
                        point->cluster_id = -1;
                        frames[CURRENT_FRAME][point->pixel.x][point->pixel.y] = equivalence_table[cluster];

                    }
                }
            }
            print_frame(frames[CURRENT_FRAME],  CURRENT_FRAME);

            //Recalculate the number of clusters
            for (uint8_t j = 0; j < clusters_points[CURRENT].count; ++j) {

                point_t *point = &clusters_points[CURRENT].points[j];

                //The biggest cluster id equals the cluster count
                clusters_points[CURRENT].clusters = MAX(clusters_points[CURRENT].clusters, point->cluster_id + 1);
            }

            //Free the alocated memory
            free(clusters_points[TEMP_CURRENT].points);
            free(clusters_points[PREVIOUS].points);
            end:

            for (uint8_t j = 0; j < clusters_points[CURRENT].count; ++j) {
                point_t *point = &clusters_points[CURRENT].points[j];
                clusters[CURRENT][point->cluster_id].size++;
            }

            //Save data for the next frame
            previous_points.count = clusters_points[CURRENT].count;
            previous_points.points = malloc(previous_points.count * sizeof(point_t));

            memset(clusters[PREVIOUS], 0, sizeof(clusters[PREVIOUS]));
            memcpy(clusters[PREVIOUS], clusters[CURRENT], sizeof(clusters[PREVIOUS]));

            for (int j = 0; j < clusters_points[CURRENT].clusters; ++j) {
                printf("[%d]%d ", j, clusters[CURRENT][j].size);
            }
            printf("\n");

            memset(previous_points.points, 0, previous_points.count * sizeof(point_t));

            for (int j = 0; j < clusters_points[CURRENT].count; ++j) {
                clusters_points[CURRENT].points[j].frame_id = PREVIOUS_FRAME;
            }

            previous_points.clusters = clusters_points[CURRENT].clusters;

            memcpy(previous_points.points, clusters_points[CURRENT].points,
                   clusters_points[CURRENT].count * sizeof(point_t));
            free(clusters_points[CURRENT].points);
            free(clusters_points[MIX].points);

#if CONFIG_OUCHAT_DEBUG_LOGGER

            size_t outlen;
            unsigned char output[190];

            mbedtls_base64_encode(output, 190, &outlen, (const unsigned char *) results.distance_mm,
                                  sizeof(results.distance_mm));

            ouchat_log(output, outlen);
#endif

            if (empty_frames != 0 || status) {
                empty_frames = status == 0 ? 0 : empty_frames + status;
            }
        }
    }

    free(previous_points.points);

    //Wait fot the api request to be compete
    while (ouchat_api_status == REQUESTING_API) {
        WaitMs(&(ouchat_sensor_configuration.platform), 20);
    }

    WaitMs(&(ouchat_sensor_configuration.platform), 2500);

#if CONFIG_OUCHAT_DEBUG_LOGGER
    //If logging is enabled end the connection
    ouchat_stop_logger();
#endif

    vl53l8cx_stop_ranging(&ouchat_sensor_configuration);

    ouchat_lp_sensor(sensor_config, threshold_config, &ouchat_sensor_background[0]);
    vl53l8cx_start_ranging(&ouchat_sensor_configuration);

    esp_deep_sleep_disable_rom_logging();

    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_NUM_3, 0));
    esp_deep_sleep_start();

}