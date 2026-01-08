#ifndef MEANSHIFT_H
#define MEANSHIFT_H

#include <stdint.h>

// Define a struct for points
#include "../type.h"

// Function declarations
double euclidean_dist(uint64_t a, uint64_t b);
void mean_shift(Point* points, int num_points, double bandwidth, double tol, int max_iter, uint64_t* centroids);
int find_closest_centroid(uint64_t point, uint64_t* centroids, int num_centroids);

#endif // MEANSHIFT_H
