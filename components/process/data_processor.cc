//
// Created by Romain on 28/01/2024.
//

#include <cmath>
#include <vector>
#include <sys/param.h>
#include <sensor.h>
#include <calibrator.h>
#include <api.h>
#include <leds.h>
#include <led_strip.h>
#include <utils.h>
#include <ai.h>

#include "data_processor.h"

static frame_t previous_frame;
static uint8_t* current_size_map[8][8];

using namespace ouchat;

static void movement_handler(tracker_t tracker, const calibration_config_t& calibration)
{
    //Prepare all the input data for tha ai
    ai::model_input input{};

    coord_t* start = &tracker.entry_coord;
    input.start_x = static_cast<float>(start->x);
    input.start_y = static_cast<float>(start->y);
    input.start_z = static_cast<float>(start->z);

    coord_t* end = &tracker.exit_coord;
    input.end_x = static_cast<float>(end->x);
    input.end_y = static_cast<float>(end->y);
    input.end_z = static_cast<float>(end->z);

    double dx = start->x - end->x;
    double dy = start->y - end->y;
    double dz = start->z - end->z;
    input.displacement_x = static_cast<float>(dx);
    input.displacement_y = static_cast<float>(dy);
    input.displacement_z = static_cast<float>(dz);

    if((dx == 0) && (dy == 0) & (dz == 0)){
        return;
    }

    input.average_height = static_cast<float>(tracker.average_height);
    input.average_deviation = static_cast<float>(tracker.average_deviation);
    input.floor_distance = static_cast<float>(calibration.floor_distance);

    coord_t* maximum = &tracker.maximum;
    coord_t* minimum = &tracker.minimum;
    input.absolute_displacement_x = static_cast<float>(maximum->x - minimum->x);
    input.absolute_displacement_y = static_cast<float>(maximum->y - minimum->y);

    input.average_size = static_cast<float>(tracker.average_size);

    bounding_box_t* entry = &tracker.entry_box;
    input.entry_box_max_x = static_cast<float>(entry->maximum.x);
    input.entry_box_max_y = static_cast<float>(entry->maximum.y);
    input.entry_box_min_x = static_cast<float>(entry->minimum.x);
    input.entry_box_min_y = static_cast<float>(entry->minimum.y);

    bounding_box_t* exit = &tracker.exit_box;
    input.exit_box_max_x = static_cast<float>(exit->maximum.x);
    input.exit_box_max_y = static_cast<float>(exit->maximum.y);
    input.exit_box_min_x = static_cast<float>(exit->minimum.x);
    input.exit_box_min_y = static_cast<float>(exit->minimum.y);

    ai::result result;

    ai::interpreter::predict(input, &result);

    TaskHandle_t xHandle = nullptr;

    if (result == ai::INSIDE)
    {
        ESP_LOGI(PROCESSOR_LOG_TAG, "\e[1;32m Inside !\e[0m");

        //Set the LED color
        set_color({.green = 50});

        int16_t value = 1;

        xTaskCreatePinnedToCore(api_set, "ouchat_api", 4096, &value, 4, &xHandle, 1);
        configASSERT(xHandle);
    }

    if (result == ai::OUTSIDE)
    {
        ESP_LOGI(PROCESSOR_LOG_TAG, "\e[1;31m Outside !\e[0m");

        //Set the LED color
        set_color({.red = 50});

        int16_t value = 0;

        xTaskCreatePinnedToCore(api_set, "ouchat_api", 4096, &value, 4, &xHandle, 1);
        configASSERT(xHandle);
    }
}

static bool is_in_the_grid(int x, int y)
{
    //Check if the point is in the grid
    if (x < 0 || x > 7)
    {
        return false;
    }
    if (y < 0 || y > 7)
    {
        return false;
    }
    return true;
}

static void evaluate_size(frame_t* frame, p_coord_t source, uint8_t* size)
{
    //This loop goes to all neighbours at a "grid distance" of 1
    for (NEIGHBOURS_FOR_LOOP)
    {
        p_coord_t neighbour;
        neighbour.x = source.x + x - 1;
        neighbour.y = source.y + y - 1;

        //Check if the point is in the grid
        if (!is_in_the_grid(neighbour.x, neighbour.y))
        {
            continue;
        }

        if (frame->data[neighbour.x][neighbour.y].cluster_id != UNDEFINED)
        {
            continue;
        }

        if (current_size_map[neighbour.x][neighbour.y] != nullptr)
        {
            continue;
        }

        const point_t* source_point = &frame->data[source.x][source.y];
        const point_t* neighbour_point = &frame->data[neighbour.x][neighbour.y];

        if (fabs(source_point->coord.z - neighbour_point->coord.z) < OBJ_CLEARANCE_MM)
        {
            (*size)++;
            current_size_map[neighbour.x][neighbour.y] = size;
            evaluate_size(frame, neighbour, size);
        }
    }
}

static void propagate_to_neighbours(frame_t* frame, p_coord_t source, cluster_t* cluster)
{
    //This loop goes to all neighbours at a "grid distance" of 1
    for (NEIGHBOURS_FOR_LOOP)
    {
        p_coord_t neighbour;
        neighbour.x = source.x + x - 1;
        neighbour.y = source.y + y - 1;

        //Check if the point is in the grid
        if (!is_in_the_grid(neighbour.x, neighbour.y))
        {
            continue;
        }

        if (frame->data[neighbour.x][neighbour.y].cluster_id != UNDEFINED)
        {
            continue;
        }

        point_t* source_point = &frame->data[source.x][source.y];
        point_t* neighbour_point = &frame->data[neighbour.x][neighbour.y];

        if (fabs(source_point->coord.z - neighbour_point->coord.z) < OBJ_CLEARANCE_MM)
        {
            neighbour_point->cluster_id = cluster->id;
            cluster->size++;
            propagate_to_neighbours(frame, neighbour, cluster);
        }
    }
}

static void frame_init(frame_t* frame, coord_t data[8][8])
{
    //Copy each point and set its cluster_id to undefined
    for (X_Y_FOR_LOOP)
    {
        memcpy(&frame->data[x][y].coord, &data[x][y], sizeof(coord_t));
        frame->data[x][y].cluster_id = UNDEFINED;
    }

    //Set the cluster / background count to 0
    frame->clusters = std::vector<cluster_t>();
    frame->background_count = 0;
}

static void frame_background_mask(frame_t* frame, const calibration_config_t& config)
{
    //All point near the floor (±150mm)
    for (X_Y_FOR_LOOP)
    {
        if (fabs(frame->data[x][y].coord.z - config.floor_distance) < BACKGROUND_THRESHOLD || config.outliers[x][y] != -
            1)
        {
            frame->data[x][y].cluster_id = BACKGROUND;
            frame->background_count++;
        }
    }
}

static int8_t nearest_neighbour(const frame_t* frame, p_coord_t coord)
{
    int16_t cluster_id = frame->data[coord.x][coord.y].cluster_id;

    for (int8_t n = 1; n < 5; ++n)
    {
        //Von Neumann neighborhood of range n
        for (int row = -n; row <= n; ++row)
        {
            int x = abs(row) - n + coord.x;
            int y = row + coord.y;

            if (!is_in_the_grid(x, y))
            {
                return n;
            }

            if (cluster_id != frame->data[x][y].cluster_id)
            {
                return n;
            }

            //Avoid repetitions on top and bottom corner
            if (n - abs(row) == 0)
            {
                continue;
            }

            //Add the symectrical
            x = n - abs(row) + coord.x;

            if (!is_in_the_grid(x, y))
            {
                return n;
            }

            if (cluster_id != frame->data[x][y].cluster_id)
            {
                return n;
            }
        }
    }
    return -1;
}

static int16_t cluster_index(const std::vector<cluster_t>& array, int16_t cluster_id)
{
    //Search in the entire array
    int16_t index = 0;
    for (auto cluster : array)
    {
        if (cluster.id == cluster_id)
        {
            return index;
        }
        ++index;
    }

    return -1;
}

static int pixel_comparator(const void* pa, const void* pb)
{
    p_coord_t a = *static_cast<const p_coord_t*>(pa);
    p_coord_t b = *static_cast<const p_coord_t*>(pb);

    point_t* point_a = &previous_frame.data[a.x][a.y];
    point_t* point_b = &previous_frame.data[b.x][b.y];

    //First compare the clusters
    int16_t index_a = cluster_index(previous_frame.clusters, point_a->cluster_id);
    int16_t index_b = cluster_index(previous_frame.clusters, point_b->cluster_id);
    cluster_t* cluster_a = &previous_frame.clusters[index_a];
    cluster_t* cluster_b = &previous_frame.clusters[index_b];

    //If the two points are not in the same cluster the biggest cluster wins
    if (cluster_a->id != cluster_b->id)
    {
        return cluster_b->size - cluster_a->size;
    }

    uint8_t futur_a;
    uint8_t futur_b;

    if (current_size_map[a.x][a.y] == nullptr)
    {
        goto neighbour;
    }

    if (current_size_map[b.x][b.y] == nullptr)
    {
        goto neighbour;
    }

    //Compare the sizes of the future cluster
    futur_a = *current_size_map[a.x][a.y];
    futur_b = *current_size_map[b.x][b.y];

    if (futur_a != futur_b)
    {
        return futur_b - futur_a;
    }

neighbour:

    //In a cluster the most centered point wins, calculated using Von Neumann neighborhood
    return nearest_neighbour(&previous_frame, b) - nearest_neighbour(&previous_frame, a);
}

esp_err_t process_init()
{
    previous_frame = {};
    return ESP_OK;
}

esp_err_t print_frame(const frame_t& frame)
{
    printf("________________\n");
    for (X_Y_FOR_LOOP)
    {
        if (frame.data[x][y].cluster_id >= 0)
        {
            printf("\e[4%im %i", frame.data[x][y].cluster_id + 1, frame.data[x][y].cluster_id);
        }
        else
        {
            printf("\e[4%im  ", 0);
        }

        if (y == 7)
        {
            printf("\e[0m\n");
        }
    }

    return ESP_OK;
}

esp_err_t process_data(coord_t sensor_data[8][8], calibration_config_t calibration)
{
    //The cluster count is 0, so it's the first frame
    if (previous_frame.clusters.empty())
    {
        frame_t first_frame;

        //Copy all the 3d points of the corrected data
        frame_init(&first_frame, sensor_data);
        frame_background_mask(&first_frame, calibration);

        //Grid DBSCAN
        int16_t count = 0;
        for (X_Y_FOR_LOOP)
        {
            //Take only point not in the background
            if (first_frame.data[x][y].cluster_id == BACKGROUND)
            {
                continue;
            }

            if (first_frame.data[x][y].cluster_id != UNDEFINED)
            {
                continue;
            }

            cluster_t cluster = {.id = count, .size = 0};

            propagate_to_neighbours(&first_frame, (p_coord_t){x, y}, &cluster);

            //Add the cluster
            first_frame.clusters.insert(first_frame.clusters.end(), cluster);

            count++;
        }

        //All the clusters are new so the trackers must be initialized
        coord_t sums[count];
        for (X_Y_FOR_LOOP)
        {
            int16_t cluster_id = first_frame.data[x][y].cluster_id;

            //Check if the point has a cluster
            if (cluster_id < 0)
            {
                continue;
            }

            coord_t coord = first_frame.data[x][y].coord;
            int16_t index = cluster_index(first_frame.clusters, cluster_id);

            if (index < 0)
            {
                continue;
            }

            //Add all coord
            sums[index].x += coord.x;
            sums[index].y += coord.y;
            sums[index].z += coord.z;

            //Set tracker bounding box
            bounding_box_t* box = &first_frame.clusters[index].tracker.entry_box;

            //Init if nesscary the bounding boxes
            if (box->maximum.empty() && box->minimum.empty())
            {
                box->maximum = coord;
                box->minimum = coord;
            }

            box->maximum.x = MAX(box->maximum.x, coord.x);
            box->maximum.y = MAX(box->maximum.y, coord.y);
            box->maximum.z = MAX(box->maximum.z, coord.z);

            box->minimum.x = MIN(box->minimum.x, coord.x);
            box->minimum.y = MIN(box->minimum.y, coord.y);
            box->minimum.z = MIN(box->minimum.z, coord.z);
        }

        //Get the average for each cluster and add it to it tracker
        coord_t start_coord[count];
        for (int i = 0; i < count; ++i)
        {
            start_coord[i].x = sums[i].x / first_frame.clusters[i].size;
            start_coord[i].y = sums[i].y / first_frame.clusters[i].size;
            start_coord[i].z = sums[i].z / first_frame.clusters[i].size;

            tracker_t* tracker = &first_frame.clusters[i].tracker;

            tracker->age = 0;
            tracker->entry_coord = start_coord[i];
            tracker->average_height = start_coord[i].z;
            tracker->average_size = first_frame.clusters[i].size;

            tracker->maximum.x = start_coord[i].x;
            tracker->maximum.y = start_coord[i].y;
            tracker->maximum.z = start_coord[i].z;

            tracker->minimum.x = start_coord[i].x;
            tracker->minimum.y = start_coord[i].y;
            tracker->minimum.z = start_coord[i].z;

            first_frame.clusters[i].coord = start_coord[i];
        }

        //Calculate the variance for the standard deviation
        double variances[count];
        memset(variances, 0, sizeof(variances));
        for (X_Y_FOR_LOOP)
        {
            int16_t cluster_id = first_frame.data[x][y].cluster_id;

            //Check if the point has a cluster
            if (cluster_id < 0)
            {
                continue;
            }

            coord_t coord = first_frame.data[x][y].coord;
            int16_t index = cluster_index(first_frame.clusters, cluster_id);

            if (index < 0)
            {
                continue;
            }

            //Add all coord
            variances[index] += pow(start_coord[index].z - coord.z, 2);
        }

        //Calculate the standard deviation
        for (int i = 0; i < count; ++i)
        {
            first_frame.clusters[i].tracker.average_deviation = sqrt(variances[i] / first_frame.clusters[i].size);
        }

        print_frame(first_frame);
        previous_frame = first_frame;

        return first_frame.clusters.empty();
    }

    frame_t current_frame;

    //Copy all the 3d points of the corrected data
    frame_init(&current_frame, sensor_data);
    frame_background_mask(&current_frame, calibration);

    //Create a pixel coord array to define the processing order
    uint8_t total_points = 64 - previous_frame.background_count;
    p_coord_t coords[total_points];

    {
        uint8_t i = 0;

        memset(coords, 0, sizeof(coords));

        //List all non-background points
        for (X_Y_FOR_LOOP)
        {
            if (previous_frame.data[x][y].cluster_id == BACKGROUND)
            {
                continue;
            }

            coords[i] = (p_coord_t){x, y};
            i++;
        }
    }

    {
        uint8_t i = 0;
        uint8_t sizes[64] = {};

        memset(current_size_map, 0, sizeof(current_size_map));

        //Get the size map for the future clusters on this frame

        for (X_Y_FOR_LOOP)
        {
            //Take only point not in the background
            if (current_frame.data[x][y].cluster_id == BACKGROUND)
            {
                continue;
            }

            if (current_size_map[x][y] == nullptr)
            {
                evaluate_size(&current_frame, {x, y}, &sizes[i]);
                i++;
            }
        }
    }

    //Sort the points for further processing
    qsort(coords, total_points, sizeof(p_coord_t), pixel_comparator);

    //Transfer existing clusters to the current frame
    for (uint8_t i = 0; i < total_points; ++i)
    {
        p_coord_t coord = coords[i];

        point_t* previous_point = &previous_frame.data[coord.x][coord.y];
        point_t* current_point = &current_frame.data[coord.x][coord.y];

        int16_t cluster_id = previous_point->cluster_id;

        //Skip if the cluster was already transferred to the current frame
        if (cluster_index(current_frame.clusters, cluster_id) != -1)
        {
            continue;
        }

        //Skip if the points are to far away
        if (fabs(previous_point->coord.z - current_point->coord.z) >= OBJ_CLEARANCE_MM)
        {
            continue;
        }

        //Transmit the tracker
        int16_t index = cluster_index(previous_frame.clusters, cluster_id);
        cluster_t cluster = {.id = cluster_id, .size = 0, .tracker = previous_frame.clusters[index].tracker};

        propagate_to_neighbours(&current_frame, coord, &cluster);

        if (cluster.size == 0)
        {
            continue;
        }

        //Add the cluster
        current_frame.clusters.insert(current_frame.clusters.end(), cluster);
    }

    //Assign a cluster to the others
    for (X_Y_FOR_LOOP)
    {
        //Take only point not in the background
        if (current_frame.data[x][y].cluster_id == BACKGROUND)
        {
            continue;
        }

        if (current_frame.data[x][y].cluster_id != UNDEFINED)
        {
            continue;
        }

        //Find an available cluster id
        int16_t cluster_id = -1;
        for (int16_t i = 0; i < 128; ++i)
        {
            //This cluster exist in the previous frame
            if (cluster_index(previous_frame.clusters, i) != -1)
            {
                continue;
            }

            //This cluster already exist in this frame
            if (cluster_index(current_frame.clusters, i) != -1)
            {
                continue;
            }

            cluster_id = i;
            break;
        }

        assert(cluster_id != -1);

        cluster_t cluster = {.id = cluster_id, .size = 0};

        propagate_to_neighbours(&current_frame, (p_coord_t){x, y}, &cluster);

        if (cluster.size == 0)
        {
            continue;
        }

        //Add the cluster
        current_frame.clusters.insert(current_frame.clusters.end(), cluster);
    }

    //Print the frame
    print_frame(current_frame);

    //Find new and dead clusters
    std::vector<int16_t> cluster_ids;

    //List all the clusters of the two frames
    for (int i = 0; i < 2; ++i)
    {
        for (auto cluster : (i == 0) ? previous_frame.clusters : current_frame.clusters)
        {
            //Check if this cluster is already in the list
            if (std::ranges::find(cluster_ids.begin(), cluster_ids.end(), cluster.id) == cluster_ids.end())
            {
                cluster_ids.insert(cluster_ids.end(), cluster.id);
            }
        }
    }

    for (auto cluster_id : cluster_ids)
    {
        int16_t previous_index = cluster_index(previous_frame.clusters, cluster_id);
        int16_t current_index = cluster_index(current_frame.clusters, cluster_id);

        cluster_t* cluster = &previous_frame.clusters[previous_index];

        //End
        if (previous_index != -1 && current_index == -1)
        {
            cluster->tracker.exit_coord.x = cluster->coord.x;
            cluster->tracker.exit_coord.y = cluster->coord.y;
            cluster->tracker.exit_coord.z = cluster->coord.z;

            cluster->tracker.exit_box = cluster->box;

            movement_handler(cluster->tracker, calibration);
        }
    }

    //Track the coordinates
    coord_t sums[current_frame.clusters.size()];
    memset(sums, 0, sizeof sums);

    for (X_Y_FOR_LOOP)
    {
        int16_t cluster_id = current_frame.data[x][y].cluster_id;

        //Check if the point has a cluster
        if (cluster_id < 0)
        {
            continue;
        }

        coord_t coord = current_frame.data[x][y].coord;
        int16_t index = cluster_index(current_frame.clusters, cluster_id);

        if (index < 0)
        {
            continue;
        }

        //Add all coord
        sums[index].x += coord.x;
        sums[index].y += coord.y;
        sums[index].z += coord.z;

        //Set tracker bounding box
        bounding_box_t* box = &current_frame.clusters[index].box;

        //Init if nesscary the bounding boxes
        if (box->maximum.empty() && box->minimum.empty())
        {
            box->maximum = coord;
            box->minimum = coord;
        }

        box->maximum.x = MAX(box->maximum.x, coord.x);
        box->maximum.y = MAX(box->maximum.y, coord.y);
        box->maximum.z = MAX(box->maximum.z, coord.z);

        box->minimum.x = MIN(box->minimum.x, coord.x);
        box->minimum.y = MIN(box->minimum.y, coord.y);
        box->minimum.z = MIN(box->minimum.z, coord.z);
    }

    //Get the average for each cluster and add it to it tracker
    for (cluster_t& cluster : current_frame.clusters)
    {
        int16_t index = cluster_index(current_frame.clusters, cluster.id);
        coord_t average_coord{};

        average_coord.x = sums[index].x / cluster.size;
        average_coord.y = sums[index].y / cluster.size;
        average_coord.z = sums[index].z / cluster.size;

        cluster.coord = average_coord;
    }

    //Calculate the variance for the standard deviation
    double variances[current_frame.clusters.size()];
    memset(variances, 0, sizeof(variances));

    for (X_Y_FOR_LOOP)
    {
        int16_t cluster_id = current_frame.data[x][y].cluster_id;

        //Check if the point has a cluster
        if (cluster_id < 0)
        {
            continue;
        }

        coord_t coord = current_frame.data[x][y].coord;
        int16_t index = cluster_index(current_frame.clusters, cluster_id);
        cluster_t cluster = current_frame.clusters[index];

        if (index < 0)
        {
            continue;
        }

        //Add all coord
        variances[index] += pow(cluster.coord.z - coord.z, 2);
    }

    for (cluster_t& cluster : current_frame.clusters)
    {
        //Check if the cluster exists on the previous frame
        bool is_new = cluster_index(previous_frame.clusters, cluster.id) == -1;
        int16_t index = cluster_index(current_frame.clusters, cluster.id);

        tracker_t* tracker = &cluster.tracker;
        double average_deviation = sqrt(variances[index] / cluster.size);

        //On new clusters tracking must be initialized
        if (is_new)
        {
            tracker->age = 0;
            tracker->entry_coord = cluster.coord;
            tracker->maximum = cluster.coord;
            tracker->minimum = cluster.coord;

            tracker->entry_box = cluster.box;
        }

        tracker->average_height = (cluster.coord.z + tracker->average_height * tracker->age) / (tracker->age + 1);
        tracker->average_deviation = (average_deviation + tracker->average_deviation * tracker->age) / (tracker->age + 1);
        tracker->average_size = (cluster.size + tracker->average_size * tracker->age) / (tracker->age + 1);
        tracker->age++;

        //Max min
        tracker->maximum.x = MAX(tracker->maximum.x, cluster.coord.x);
        tracker->maximum.y = MAX(tracker->maximum.y, cluster.coord.y);
        tracker->maximum.z = MAX(tracker->maximum.z, cluster.coord.z);

        tracker->minimum.x = MIN(tracker->minimum.x, cluster.coord.x);;
        tracker->minimum.y = MIN(tracker->minimum.y, cluster.coord.y);;
        tracker->minimum.z = MIN(tracker->minimum.z, cluster.coord.z);;
    }

    previous_frame = current_frame;

    print_frame(current_frame);

    return current_frame.clusters.empty();
}
