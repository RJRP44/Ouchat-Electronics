/* Copyright 2015 Gagarine Yaikhom (MIT License) */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbscan.h>
#include "ouchat_units.h"
#include "ouchat_calibrator.h"

double get_epsilon(point_t point, double epsilon) {
    double distance = 0;
    pixel_angles_t angles[2];

    pixel_coordinates_t a = {point.pixel.x < 0 ? point.pixel.x - 1 : point.pixel.x + 1, point.pixel.y};

    pixel_angles(point.pixel, &angles[0]);
    pixel_angles(a, &angles[1]);

    return EPSILON_COEFFICIENT * sin(angles[0].x - angles[1].x) * (point.measurement / (sin(epsilon - angles[1].x)));
}

node_t *create_node(unsigned int index) {
    node_t *n = (node_t *) calloc(1, sizeof(node_t));
    if (n == NULL)
        perror("Failed to allocate node.");
    else {
        n->index = index;
        n->next = NULL;
    }
    return n;
}

int append_at_end(unsigned int index, epsilon_neighbours_t *en) {
    node_t *n = create_node(index);
    if (n == NULL) {
        free(en);
        return FAILURE;
    }
    if (en->head == NULL) {
        en->head = n;
        en->tail = n;
    } else {
        en->tail->next = n;
        en->tail = n;
    }
    ++(en->num_members);
    return SUCCESS;
}

epsilon_neighbours_t *
get_epsilon_neighbours(unsigned int index, point_t *points, unsigned int num_points, double epsilon,
                       double (*dist)(point_t *a, point_t *b)) {
    epsilon_neighbours_t *en = (epsilon_neighbours_t *) calloc(1, sizeof(epsilon_neighbours_t));
    if (en == NULL) {
        perror("Failed to allocate epsilon neighbours.");
        return en;
    }
    for (int i = 0; i < num_points; ++i) {
        if (i == index)
            continue;
        if (dist(&points[index], &points[i]) > get_epsilon(points[i], epsilon))
            continue;
        else {
            if (append_at_end(i, en) == FAILURE) {
                destroy_epsilon_neighbours(en);
                en = NULL;
                break;
            }
        }
    }
    return en;
}

void destroy_epsilon_neighbours(epsilon_neighbours_t *en) {
    if (en) {
        node_t *t, *h = en->head;
        while (h) {
            t = h->next;
            free(h);
            h = t;
        }
        free(en);
    }
}

void dbscan(cluster_points_t clusters_point, double epsilon, unsigned int minpts, double (*dist)(point_t *a, point_t *b)) {
    int8_t cluster_id = 0;
    for (int i = 0; i < clusters_point.count; ++i) {
        if (clusters_point.points[i].cluster_id == UNCLASSIFIED) {
            if (expand(i, cluster_id, clusters_point.points, clusters_point.count, epsilon, minpts, dist) == CORE_POINT)
                ++cluster_id;
        }
    }
}

int expand(unsigned int index, int8_t cluster_id, point_t *points, unsigned int num_points, double epsilon,
           unsigned int minpts, double (*dist)(point_t *a, point_t *b)) {
    int return_value = NOT_CORE_POINT;
    epsilon_neighbours_t *seeds = get_epsilon_neighbours(index, points, num_points, epsilon, dist);
    if (seeds == NULL)
        return FAILURE;

    if (seeds->num_members < minpts)
        points[index].cluster_id = NOISE;
    else {
        points[index].cluster_id = cluster_id;
        node_t *h = seeds->head;
        while (h) {
            points[h->index].cluster_id = cluster_id;
            h = h->next;
        }

        h = seeds->head;
        while (h) {
            spread(h->index, seeds, cluster_id, points, num_points, epsilon, minpts, dist);
            h = h->next;
        }

        return_value = CORE_POINT;
    }
    destroy_epsilon_neighbours(seeds);
    return return_value;
}

int spread(unsigned int index, epsilon_neighbours_t *seeds, int8_t cluster_id, point_t *points, unsigned int num_points,
           double epsilon, unsigned int minpts, double (*dist)(point_t *a, point_t *b)) {
    epsilon_neighbours_t *spread = get_epsilon_neighbours(index, points, num_points, epsilon, dist);

    if (spread == NULL)
        return FAILURE;
    if (spread->num_members >= minpts) {
        node_t *n = spread->head;
        point_t *d;
        while (n) {
            d = &points[n->index];
            if (d->cluster_id == NOISE || d->cluster_id == UNCLASSIFIED) {
                if (d->cluster_id == UNCLASSIFIED) {
                    if (append_at_end(n->index, seeds) == FAILURE) {
                        destroy_epsilon_neighbours(spread);
                        return FAILURE;
                    }
                }
                d->cluster_id = cluster_id;
            }
            n = n->next;
        }
    }

    destroy_epsilon_neighbours(spread);
    return SUCCESS;
}

double euclidean_dist(point_t *a, point_t *b) {
    return sqrt(pow(a->coordinates.x - b->coordinates.x, 2) +
                pow(a->coordinates.y - b->coordinates.y, 2) +
                pow(a->coordinates.z - b->coordinates.z, 2));
}