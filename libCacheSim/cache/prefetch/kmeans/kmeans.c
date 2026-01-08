#include "kmeans.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Function to calculate the Euclidean distance between two points
double euclidean_distance(double x1, double x2) {
    return fabs(x1 - x2);
}

// Function to initialize centroids randomly
void initialize_centroids(Centroid *centroids, Point *points, int num_clusters, int np) {
    for (int i = 0; i < num_clusters; i++) {
        centroids[i].x = points[rand() % np].x;
    }
}

// Function to assign points to the nearest centroid
void assign_clusters(Point *points, Centroid *centroids, int num_clusters, int np) {
    for (int i = 0; i < np; i++) {
        double min_dist = euclidean_distance(points[i].x, centroids[0].x);
        points[i].cluster_id = 0;

        for (int j = 1; j < num_clusters; j++) {
            double dist = euclidean_distance(points[i].x, centroids[j].x);
            if (dist < min_dist) {
                min_dist = dist;
                points[i].cluster_id = j;
            }
        }
    }
}

// Function to update centroids based on the assigned clusters
void update_centroids(Point *points, Centroid *centroids, int num_clusters, int np) {
    int *cluster_sizes = (int *)calloc(num_clusters, sizeof(int));
    double *new_centroid_x = (double *)calloc(num_clusters, sizeof(double));

    // Sum up the points for each cluster
    for (int i = 0; i < np; i++) {
        int cluster_id = points[i].cluster_id;
        new_centroid_x[cluster_id] += points[i].x;
        cluster_sizes[cluster_id]++;
    }

    // Calculate new centroid positions
    for (int j = 0; j < num_clusters; j++) {
        if (cluster_sizes[j] > 0) {
            centroids[j].x = new_centroid_x[j] / cluster_sizes[j];
        }
    }

    free(cluster_sizes);
    free(new_centroid_x);
}

// K-means clustering function
void kmeans(Point *points, int num_clusters, int np) {
    Centroid *centroids = (Centroid *)malloc(num_clusters * sizeof(Centroid));
    initialize_centroids(centroids, points, num_clusters,np);

    for (int iter = 0; iter < MAX_ITER; iter++) {
        assign_clusters(points, centroids, num_clusters,np);
        update_centroids(points, centroids, num_clusters,np);
    }

    free(centroids);
}

// Function to get the cluster ID of a specific point by its value
int get_cluster_id_kmeans(Point *points, int num_points, uint64_t point_value) {
    for (int i = 0; i < num_points; i++) {
        if ((uint64_t)points[i].x == point_value) {
            return points[i].cluster_id;
        }
    }
    return -1; // Return -1 if the point is not found
}
