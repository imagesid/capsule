#ifndef KMEANS_H
#define KMEANS_H

#include <stdint.h> // For uint64_t


#define NUM_CLUSTERS 5
#define MAX_ITER 100

#include "../type.h"


typedef struct {
    double x;
} Centroid;

double euclidean_distance(double x1, double x2);
void initialize_centroids(Centroid *centroids, Point *points, int num_clusters, int np);
void assign_clusters(Point *points, Centroid *centroids, int num_clusters, int np);
void update_centroids(Point *points, Centroid *centroids, int num_clusters, int np);
void kmeans(Point *points, int num_clusters, int np);

// New functions
int get_cluster_id_kmeans(Point *points, int num_points, uint64_t point_value);
void print_points_in_cluster(Point *points, int num_points, int cluster_id);

#endif
