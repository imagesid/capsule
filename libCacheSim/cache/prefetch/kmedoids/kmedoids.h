#ifndef KMEDOIDS_H
#define KMEDOIDS_H

#include <stdint.h>

// Define a struct for points
#include "../type.h"

// Function declarations
uint64_t manhattan_dist(uint64_t a, uint64_t b);
void k_medoids(Point* points, int num_points, int k, int max_iter, uint64_t* medoids);
void assign_points_to_medoids(Point* points, int num_points, uint64_t* medoids, int k);
uint64_t compute_cost(Point* points, int num_points, uint64_t* medoids, int k);

#endif // KMEDOIDS_H
