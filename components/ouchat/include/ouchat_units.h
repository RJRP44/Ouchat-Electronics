//
// Created by Romain on 29/12/2023.
//

#ifndef OUCHAT_DATA_PREPROCESSING_OUCHAT_UNITS_H
#define OUCHAT_DATA_PREPROCESSING_OUCHAT_UNITS_H

#include <stdint.h>

typedef struct {
    uint8_t x;
    uint8_t y;
} pixel_coordinates_t;

typedef struct {
    double x, y, z;
} coordinates_t;

typedef struct {
    coordinates_t coordinates;
    pixel_coordinates_t pixel;
    int8_t cluster_id;
    uint8_t frame_id;
    double measurement;
} point_t;

typedef struct {
    uint8_t count;
    point_t *points;
    uint8_t clusters;
} cluster_points_t;

typedef struct {
    uint8_t size;
} cluster_t;

typedef struct node_s node_t;
struct node_s {
    unsigned int index;
    node_t *next;
};

typedef struct {
    unsigned int num_members;
    node_t *head, *tail;
} epsilon_neighbours_t;

typedef struct {
    double x, y;
} pixel_angles_t;

typedef struct {
    double a, b, c, d;
} plane_t;

typedef struct {
    double x, y, z;
} plane_angles_t;

#endif //OUCHAT_DATA_PREPROCESSING_OUCHAT_UNITS_H
