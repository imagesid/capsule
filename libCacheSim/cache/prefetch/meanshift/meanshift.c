#include "meanshift.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double euclidean_dist(uint64_t a, uint64_t b) {
    return fabs((double)a - (double)b);
}

void mean_shift(Point* points, int num_points, double bandwidth, double tol, int max_iter, uint64_t* centroids) {
    // Initialize centroids as a copy of points
    for (int i = 0; i < num_points; i++) {
        centroids[i] = points[i].x;
    }

    uint64_t* new_centroids = (uint64_t*)malloc(num_points * sizeof(uint64_t));

    for (int iteration = 0; iteration < max_iter; iteration++) {
        int converged = 1;

        for (int i = 0; i < num_points; i++) {
            uint64_t sum = 0;
            int count = 0;

            // Mean shift step: find points within the bandwidth
            for (int j = 0; j < num_points; j++) {
                if (euclidean_dist(points[j].x, centroids[i]) < bandwidth) {
                    sum += points[j].x;
                    count++;
                }
            }

            if (count > 0) {
                new_centroids[i] = sum / count; // Shift towards the mean
            } else {
                new_centroids[i] = centroids[i];
            }

            // Check for convergence
            if (fabs((double)new_centroids[i] - (double)centroids[i]) > tol) {
                converged = 0;
            }
        }

        // Copy new centroids to centroids
        for (int i = 0; i < num_points; i++) {
            centroids[i] = new_centroids[i];
        }

        if (converged) {
            printf("Converged after %d iterations.\n", iteration + 1);
            break;
        }
    }

    // Assign each point to its closest centroid
    for (int i = 0; i < num_points; i++) {
        points[i].cluster_id = find_closest_centroid(points[i].x, centroids, num_points);
    }

    free(new_centroids);
}

int find_closest_centroid(uint64_t point, uint64_t* centroids, int num_centroids) {
    int closest_index = 0;
    double closest_dist = euclidean_dist(point, centroids[0]);

    for (int i = 1; i < num_centroids; i++) {
        double dist = euclidean_dist(point, centroids[i]);
        if (dist < closest_dist) {
            closest_dist = dist;
            closest_index = i;
        }
    }

    return closest_index;
}
