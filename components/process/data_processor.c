//
// Created by Romain on 28/01/2024.
//

#include <stdbool.h>
#include <ouchat_utils.h>
#include <math.h>
#include <memory.h>
#include <sensor.h>
#include "include/data_processor.h"


static frame_t previous_frame;

static bool is_in_the_grid(int x, int y) {
    //Check if the point is in the grid
    if (x < 0 || x > 7) {
        return false;
    }
    if (y < 0 || y > 7) {
        return false;
    }
    return true;
}

static void propagate_to_neighbours(frame_t *frame, p_coord_t source, cluster_t *cluster) {

    //This loop goes to all neighbours at a "grid distance" of 1
    for (NEIGHBOURS_FOR_LOOP) {
        p_coord_t neighbour;
        neighbour.x = source.x + x - 1;
        neighbour.y = source.y + y - 1;

        //Check if the point is in the grid
        if (!is_in_the_grid(neighbour.x, neighbour.y)) {
            continue;
        }

        if (frame->data[neighbour.x][neighbour.y].cluster_id != UNDEFINED) {
            continue;
        }

        point_t *source_point = &frame->data[source.x][source.y];
        point_t *neighbour_point = &frame->data[neighbour.x][neighbour.y];

        if (fabs(source_point->coord.z - neighbour_point->coord.z) < OBJ_CLEARANCE_MM) {
            neighbour_point->cluster_id = cluster->id;
            cluster->size++;
            propagate_to_neighbours(frame, neighbour, cluster);
        }
    }
}

static void frame_init(frame_t *frame, coord_t data[8][8]) {

    //Copy each point and set its cluster_id to undefined
    for (X_Y_FOR_LOOP) {
        memcpy(&frame->data[x][y].coord, &data[x][y], sizeof(coord_t));
        frame->data[x][y].cluster_id = UNDEFINED;
    }

    //Set the cluster / background count to 0
    frame->clusters_count = 0;
    frame->background_count = 0;
}

static void frame_background_mask(frame_t *frame, calibration_config_t config, int16_t threshold) {

    //All point near the floor (±150mm)
    for (X_Y_FOR_LOOP) {
        if (fabs(frame->data[x][y].coord.z - config.floor_distance) < threshold) {
            frame->data[x][y].cluster_id = BACKGROUND;
            frame->background_count++;
        }
    }
}

static int8_t nearest_neighbour(frame_t *frame, p_coord_t coord) {

    int16_t cluster_id = frame->data[coord.x][coord.y].cluster_id;

    for (int8_t n = 1; n < 5; ++n) {

        //Von Neumann neighborhood of range n
        for (int row = -n; row <= n; ++row) {

            int x = abs(row) - n + coord.x;
            int y = row + coord.y;

            if (!is_in_the_grid(x, y)) {
                return n;
            }

            if (cluster_id != frame->data[x][y].cluster_id) {
                return n;
            }

            //Avoid repetitions on top and bottom corner
            if (n - abs(row) == 0) {
                continue;
            }

            //Add the symectrical
            x = n - abs(row) + coord.x;

            if (!is_in_the_grid(x, y)) {
                return n;
            }

            if (cluster_id != frame->data[x][y].cluster_id) {
                return n;
            }
        }
    }
    return -1;
}

int16_t cluster_index(const cluster_t *array, int16_t cluster_id, uint8_t length){
    //Search in the entire array
    for (uint8_t i = 0; i < length; i++) {
        if (array[i].id == cluster_id) {
            return i;
        }
    }
    return -1;
}

static int pixel_comparator(const void *pa, const void *pb) {
    p_coord_t a = *(p_coord_t *) pa;
    p_coord_t b = *(p_coord_t *) pb;

    point_t *point_a = &previous_frame.data[a.x][a.y];
    point_t *point_b = &previous_frame.data[b.x][b.y];

    //First compare the clusters
    int16_t index_a = cluster_index(previous_frame.clusters,point_a->cluster_id,previous_frame.clusters_count);
    int16_t index_b = cluster_index(previous_frame.clusters,point_b->cluster_id,previous_frame.clusters_count);
    cluster_t *cluster_a = &previous_frame.clusters[index_a];
    cluster_t *cluster_b = &previous_frame.clusters[index_b];

    //If the two points are not in the same cluster the biggest cluster wins
    if (cluster_a->id != cluster_b->id) {
        return cluster_b->size - cluster_a->size;
    }

    //In a cluster the most centered point wins, calculated using Von Neumann neighborhood
    return nearest_neighbour(&previous_frame, b) - nearest_neighbour(&previous_frame, a);
}

esp_err_t process_init() {
    memset(&previous_frame, 0, sizeof(previous_frame));
    previous_frame.clusters_count = -1;
    return ESP_OK;
}

esp_err_t print_frame(frame_t frame) {

    printf("________________\n");
    for (X_Y_FOR_LOOP) {
        if (frame.data[x][y].cluster_id >= 0) {
            printf("\e[4%im %i", frame.data[x][y].cluster_id + 1, frame.data[x][y].cluster_id);
        } else {
            printf("\e[4%im  ", 0);
        }

        if (y == 7) {
            printf("\e[0m\n");
        }
    }

    return ESP_OK;
}

esp_err_t process_data(coord_t sensor_data[8][8], calibration_config_t calibration) {

    //The cluster count is -1, so it's the first frame
    if (previous_frame.clusters_count == -1) {

        frame_t first_frame;

        //Copy all the 3d points of the corrected data
        frame_init(&first_frame, sensor_data);
        frame_background_mask(&first_frame, calibration, BACKGROUND_THRESHOLD);

        //Grid DBSCAN
        int8_t count = 0;
        cluster_t *clusters;
        clusters = malloc(sizeof(cluster_t));

        for (X_Y_FOR_LOOP) {
            //Take only point not in the background
            if (first_frame.data[x][y].cluster_id == BACKGROUND) {
                continue;
            }

            if (first_frame.data[x][y].cluster_id != UNDEFINED) {
                continue;
            }

            cluster_t cluster = {.id = count, .size = 0};

            propagate_to_neighbours(&first_frame, (p_coord_t) {x, y}, &cluster);

            //Increase the size of the array
            cluster_t *temp = (cluster_t *) realloc(clusters, (count + 1) * sizeof(cluster_t));
            assert(temp != NULL);

            //Add the cluster
            clusters = temp;
            memcpy(&clusters[count], &cluster, sizeof(cluster_t));
            count++;
        }

        first_frame.clusters_count = count;
        first_frame.clusters = clusters;

        print_frame(first_frame);

        for (int i = 0; i < first_frame.clusters_count; ++i) {
            printf(" id : %d, size : %d\n", first_frame.clusters[i].id, first_frame.clusters[i].size);
        }
        memcpy(&previous_frame, &first_frame, sizeof(first_frame));
        return ESP_OK;
    }

    frame_t current_frame;

    //Copy all the 3d points of the corrected data
    frame_init(&current_frame, sensor_data);
    frame_background_mask(&current_frame, calibration, BACKGROUND_THRESHOLD);

    //Create a pixel coord array to define the processing order
    uint8_t total_points = 64 - previous_frame.background_count;
    p_coord_t coords[total_points];

    {
        uint8_t i = 0;

        memset(coords, 0, sizeof(coords));

        //List all non-background points
        for (X_Y_FOR_LOOP) {
            if (previous_frame.data[x][y].cluster_id == BACKGROUND) {
                continue;
            }

            coords[i] = (p_coord_t) {x, y};
            i++;
        }
    }

    //Sort the points for further processing
    qsort(coords, total_points, sizeof(p_coord_t), pixel_comparator);

    int8_t count = 0;
    cluster_t *clusters;
    clusters = malloc(sizeof(cluster_t));

    //Transfer existing clusters to the current frame
    for (uint8_t i = 0; i < total_points; ++i) {
        p_coord_t coord = coords[i];

        point_t *previous_point = &previous_frame.data[coord.x][coord.y];
        point_t *current_point = &current_frame.data[coord.x][coord.y];

        int16_t cluster_id = previous_point->cluster_id;

        //Skip if the cluster was already transferred to the current frame
        if (cluster_index(clusters, cluster_id, count) != -1) {
            continue;
        }

        //Skip if the points are to far away
        if (fabs(previous_point->coord.z - current_point->coord.z) >= OBJ_CLEARANCE_MM) {
            continue;
        }

        cluster_t cluster = {.id = cluster_id, .size = 0};

        propagate_to_neighbours(&current_frame, coord, &cluster);

        //Increase the size of the array
        cluster_t *temp = (cluster_t *) realloc(clusters, (count + 1) * sizeof(cluster_t));
        assert(temp != NULL);

        //Add the cluster
        clusters = temp;
        memcpy(&clusters[count], &cluster, sizeof(cluster_t));
        count++;
    }

    //Assign a cluster to the others
    for (X_Y_FOR_LOOP) {
        //Take only point not in the background
        if (current_frame.data[x][y].cluster_id == BACKGROUND) {
            continue;
        }

        if (current_frame.data[x][y].cluster_id != UNDEFINED) {
            continue;
        }

        //Find an available cluster id
        int16_t cluster_id = -1;
        for (int16_t i = 0; i < 128; ++i) {

            //This cluster exist in the previous frame
            if (cluster_index(previous_frame.clusters, i, previous_frame.clusters_count) != -1) {
                continue;
            }

            //This cluster already exist in this frame
            if (cluster_index(clusters, i, count) != -1) {
                continue;
            }

            cluster_id = i;
            break;
        }

        assert(cluster_id != -1);

        cluster_t cluster = {.id = cluster_id, .size = 0};

        propagate_to_neighbours(&current_frame, (p_coord_t) {x, y}, &cluster);

        //Increase the size of the array
        cluster_t *temp = (cluster_t *) realloc(clusters, (count + 1) * sizeof(cluster_t));
        assert(temp != NULL);

        //Add the cluster
        clusters = temp;
        memcpy(&clusters[count], &cluster, sizeof(cluster_t));
        count++;
    }

    current_frame.clusters_count = count;
    current_frame.clusters = clusters;

    free(previous_frame.clusters);

    memcpy(&previous_frame, &current_frame, sizeof(current_frame));

    print_frame(current_frame);

    return current_frame.clusters_count == 0;
}