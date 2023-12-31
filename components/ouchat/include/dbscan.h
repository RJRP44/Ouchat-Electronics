//
// Created by Romain on 28/12/2023.
//

#ifndef OUCHAT_DATA_PREPROCESSING_DBSCAN_H
#define OUCHAT_DATA_PREPROCESSING_DBSCAN_H

#include "ouchat_units.h"

#define EPSILON_COEFFICIENT 1

#define UNCLASSIFIED (-1)
#define NOISE (-2)

#define CORE_POINT 1
#define NOT_CORE_POINT 0

#define SUCCESS 0
#define FAILURE (-3)

node_t *create_node(unsigned int index);
int append_at_end(unsigned int index, epsilon_neighbours_t *en);
epsilon_neighbours_t *get_epsilon_neighbours(unsigned int index, point_t *points, unsigned int num_points, double epsilon,double (*dist)(point_t *a, point_t *b));
void destroy_epsilon_neighbours(epsilon_neighbours_t *en);
void dbscan(cluster_points_t clusters_point, double epsilon, unsigned int minpts, double (*dist)(point_t *a, point_t *b));
int expand(unsigned int index, int8_t cluster_id, point_t *points, unsigned int num_points, double epsilon, unsigned int minpts, double (*dist)(point_t *a, point_t *b));
int spread(unsigned int index, epsilon_neighbours_t *seeds, int8_t cluster_id, point_t *points, unsigned int num_points, double epsilon, unsigned int minpts, double (*dist)(point_t *a, point_t *b));
double euclidean_dist(point_t *a, point_t *b);
double adjacent_intensity_dist(point_t *a, point_t *b);

#endif //OUCHAT_DATA_PREPROCESSING_DBSCAN_H
