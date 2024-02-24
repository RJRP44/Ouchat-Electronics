//
// Created by Romain on 28/01/2024.
//

#include <stdbool.h>
#include <ouchat_utils.h>
#include <math.h>
#include <memory.h>
#include <sensor.h>
#include <calibrator.h>
#include "data_processor.h"

static frame_t previous_frame;

static void movement_handler(tracker_t tracker, calibration_config_t calibration){
    double dx, dy, dz;
    double adx, ady;
    coord_t *start = &tracker.entry_coord;
    coord_t *end = &tracker.exit_coord;
    dx = start->x - end->x;
    dy = start->y - end->y;
    dz = start->z - end->z;

    coord_t *maximum = &tracker.maximum;
    coord_t *minimum = &tracker.minimum;
    adx = maximum->x - minimum->x;
    ady = maximum->y - minimum->y;

    if (dx == 0 && dy == 0 && dz == 0){
        return;
    }

    bool possible_out, possible_in;

    //Check if the exit of the fov is throw the cat flap
    possible_out = sqrt(pow(tracker.exit_coord.x, 2) + pow(tracker.exit_coord.y, 2)) < CAT_FLAP_RADIUS && dy > 0;

    //Check if the entry of the fov is throw the cat flap
    possible_in = tracker.entry_coord.x < CAT_FLAP_RADIUS && dy < 0;

    if (possible_out == possible_in){
        return;
    }

    //Cat height
    if (calibration.floor_distance-tracker.average_height > MAX_CAT_HEIGHT){
        return;
    }

    //Average deviation
    if (tracker.average_deviation > 50){
        return;
    }

    coord_t *furthest_point = possible_out ? &tracker.entry_coord : &tracker.exit_coord;

    //The distance between the start of the FOV and the cat flap
    double distance_wall_fov = tan(calibration.angles.z + VL53L8CX_SENSOR_FOV / 2 * PI / 180) * - calibration.floor_distance;
    double direct_path = sqrt(pow(furthest_point->x,2)+pow(furthest_point->y - distance_wall_fov,2));
    double absolute_path = sqrt(pow(adx,2)+pow(ady,2));

    if (possible_in && (dy < -300 || absolute_path > direct_path)){
        ESP_LOGI(PROCESSOR_LOG_TAG, "\e[1;32m Inside !\e[0m");
    }

    if (possible_out && (dy > 300 || absolute_path > direct_path)){
        ESP_LOGI(PROCESSOR_LOG_TAG, "\e[1;31m Outside !\e[0m");
    }
}

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
        int16_t count = 0;
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

        //All the clusters are new so the trackers must be initialized
        coord_t sums[count];
        for (X_Y_FOR_LOOP){

            int16_t cluster_id = first_frame.data[x][y].cluster_id;

            //Check if the point has a cluster
            if (cluster_id < 0){
                continue;
            }

            coord_t coord = first_frame.data[x][y].coord;
            int16_t index = cluster_index(first_frame.clusters, cluster_id, first_frame.clusters_count);

            if (index < 0){
                continue;
            }

            //Add all coord
            sums[index].x += coord.x;
            sums[index].y += coord.y;
            sums[index].z += coord.z;
        }

        //Get the average for each cluster and add it to it tracker
        coord_t start_coord[count];
        for (int i = 0; i < count; ++i) {
            start_coord[i].x = sums[i].x / first_frame.clusters[i].size;
            start_coord[i].y = sums[i].y / first_frame.clusters[i].size;
            start_coord[i].z = sums[i].z / first_frame.clusters[i].size;

            first_frame.clusters[i].tracker.age = 0;
            first_frame.clusters[i].tracker.entry_coord = start_coord[i];
            first_frame.clusters[i].tracker.average_height = start_coord[i].z;
        }

        //Calculate the variance for the standard deviation
        double variances[count];
        for (X_Y_FOR_LOOP){

            int16_t cluster_id = first_frame.data[x][y].cluster_id;

            //Check if the point has a cluster
            if (cluster_id < 0){
                continue;
            }

            coord_t coord = first_frame.data[x][y].coord;
            int16_t index = cluster_index(first_frame.clusters, cluster_id, first_frame.clusters_count);

            if (index < 0){
                continue;
            }

            //Add all coord
            variances[index] += pow(start_coord[index].z - coord.z ,2);
        }

        //Calculate the standard deviation
        for (int i = 0; i < count; ++i) {
            first_frame.clusters[i].tracker.average_deviation = sqrt(variances[i]/first_frame.clusters[i].size);
        }

        print_frame(first_frame);
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

    int16_t count = 0;
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
        if (fabs(previous_point->coord.z - current_point->coord.z) >= OBJ_CLEARANCE_MM){
            continue;
        }

        //Transmit the tracker
        int16_t index = cluster_index(previous_frame.clusters, cluster_id, previous_frame.clusters_count);
        cluster_t cluster = {.id = cluster_id, .size = 0, .tracker = previous_frame.clusters[index].tracker};

        propagate_to_neighbours(&current_frame, coord, &cluster);

        if (cluster.size == 0){
            continue;
        }

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
            if (cluster_index(previous_frame.clusters,i,previous_frame.clusters_count) != -1){
                continue;
            }

            //This cluster already exist in this frame
            if (cluster_index(clusters,i,count) != -1){
                continue;
            }

            cluster_id = i;
            break;
        }

        assert(cluster_id != -1);

        cluster_t cluster = {.id = cluster_id, .size = 0};

        propagate_to_neighbours(&current_frame, (p_coord_t) {x, y}, &cluster);

        if (clusters->size == 0){
            continue;
        }

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

    //Print the frame
    print_frame(current_frame);

    //Find new and dead clusters
    uint8_t cluster_ids_total = 0;
    int16_t *cluster_ids;
    cluster_ids = malloc(sizeof(int16_t));

    //List all the clusters of the two frames
    for (int i = 0; i < 2; ++i) {

        cluster_t *cluster_list = (i == 0) ? previous_frame.clusters : current_frame.clusters;
        uint16_t clusters_count = (i == 0) ? previous_frame.clusters_count : current_frame.clusters_count;

        for (int j = 0; j < clusters_count; ++j) {

            bool exists = 0;

            //Check if this cluster is already in the list
            for (int k = 0; k < cluster_ids_total; ++k) {
                if (cluster_list[j].id == cluster_ids[k]){
                    exists = 1;
                    break;
                }
            }

            if (exists){
                continue;
            }

            //Add the cluster id
            int16_t *temp = realloc(cluster_ids, (cluster_ids_total + 1) * sizeof(int16_t));
            assert(temp != NULL);

            cluster_ids = temp;
            cluster_ids[cluster_ids_total] = cluster_list[j].id;
            cluster_ids_total++;
        }
    }

    for (int i = 0; i < cluster_ids_total; ++i) {

        int16_t cluster_id = cluster_ids[i];
        int16_t previous_index = cluster_index(previous_frame.clusters,cluster_id,previous_frame.clusters_count) ;
        int16_t current_index = cluster_index(current_frame.clusters,cluster_id,current_frame.clusters_count);

        cluster_t *cluster = &previous_frame.clusters[previous_index];

        //End
        if (previous_index != -1 && current_index == -1){

            cluster->tracker.exit_coord.x = cluster->coord.x;
            cluster->tracker.exit_coord.y = cluster->coord.y;
            cluster->tracker.exit_coord.z = cluster->coord.z;

            movement_handler(cluster->tracker, calibration);
        }
    }

    //Track the coordinates
    coord_t sums[current_frame.clusters_count];
    memset(sums, 0, sizeof sums);

    for (X_Y_FOR_LOOP){
        int16_t cluster_id = current_frame.data[x][y].cluster_id;

        //Check if the point has a cluster
        if (cluster_id < 0){
            continue;
        }

        coord_t coord = current_frame.data[x][y].coord;
        int16_t index = cluster_index(current_frame.clusters, cluster_id, current_frame.clusters_count);

        if (index < 0){
            continue;
        }

        //Add all coord
        sums[index].x += coord.x;
        sums[index].y += coord.y;
        sums[index].z += coord.z;
    }

    //Get the average for each cluster and add it to it tracker
    coord_t average_coords[current_frame.clusters_count];
    for (int i = 0; i < count; ++i) {
        average_coords[i].x = sums[i].x / current_frame.clusters[i].size;
        average_coords[i].y = sums[i].y / current_frame.clusters[i].size;
        average_coords[i].z = sums[i].z / current_frame.clusters[i].size;

        current_frame.clusters[i].coord = average_coords[i];

        /*
        if (current_frame.clusters[i].id == 0){
            FILE *fptr;

// Open a file in append mode
            fptr = fopen("filename.txt", "a");

// Append some text to the file
            fprintf(fptr, "%f,%f,%f\n",average_coords[i].x,average_coords[i].y,average_coords[i].z);

// Close the file
            fclose(fptr);
        }*/
    }

    //Calculate the variance for the standard deviation
    double variances[current_frame.clusters_count];
    for (X_Y_FOR_LOOP){

        int16_t cluster_id = current_frame.data[x][y].cluster_id;

        //Check if the point has a cluster
        if (cluster_id < 0){
            continue;
        }

        coord_t coord = current_frame.data[x][y].coord;
        int16_t index = cluster_index(current_frame.clusters, cluster_id, current_frame.clusters_count);

        if (index < 0){
            continue;
        }

        //Add all coord
        variances[index] += pow(average_coords[index].z - coord.z ,2);
    }

    //Calculate the standard deviation and add all the data to the tracker
    for (int i = 0; i < current_frame.clusters_count; ++i) {

        //Check if the cluster exists on the previous frame
        bool is_new = cluster_index(previous_frame.clusters,current_frame.clusters[i].id,previous_frame.clusters_count) == -1;

        tracker_t *tracker = &current_frame.clusters[i].tracker;
        double average_deviation = sqrt(variances[i]/current_frame.clusters[i].size);

        //On new clusters tracking must be initialized
        if (is_new){
            tracker->age = 0;
            tracker->entry_coord = average_coords[i];

            tracker->maximum.x = average_coords[i].x;
            tracker->maximum.y = average_coords[i].y;
            tracker->maximum.z = average_coords[i].z;

            tracker->minimum.x = average_coords[i].x;
            tracker->minimum.y = average_coords[i].y;
            tracker->minimum.z = average_coords[i].z;
        }

        tracker->average_height = (average_coords[i].z + tracker->average_height * tracker->age) / (tracker->age + 1);
        tracker->average_deviation = (average_deviation + tracker->average_deviation * tracker->age) / (tracker->age + 1);
        tracker->age++;

        //Max min
        tracker->maximum.x = tracker->maximum.x < average_coords[i].x ? average_coords[i].x : tracker->maximum.x;
        tracker->maximum.y = tracker->maximum.y < average_coords[i].y ? average_coords[i].y : tracker->maximum.y;
        tracker->maximum.z = tracker->maximum.z < average_coords[i].z ? average_coords[i].z : tracker->maximum.z;

        tracker->minimum.x = tracker->minimum.x > average_coords[i].x ? average_coords[i].x : tracker->minimum.x;
        tracker->minimum.y = tracker->minimum.y > average_coords[i].y ? average_coords[i].y : tracker->minimum.y;
        tracker->minimum.z = tracker->minimum.z > average_coords[i].z ? average_coords[i].z : tracker->minimum.z;
    }

    free(previous_frame.clusters);
    memcpy(&previous_frame, &current_frame, sizeof(current_frame));

    print_frame(current_frame);

    return current_frame.clusters_count == 0;
}