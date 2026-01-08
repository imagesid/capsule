#include "kmedoids.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

uint64_t manhattan_dist(uint64_t a, uint64_t b) {
    return abs((int64_t)(a - b));
}

void assign_points_to_medoids(Point* points, int num_points, uint64_t* medoids, int k) {
    // Assign each point to the nearest medoid
    for (int i = 0; i < num_points; i++) {
        uint64_t min_dist = manhattan_dist(points[i].x, medoids[0]);
        int closest_medoid = 0;

        for (int j = 1; j < k; j++) {
            uint64_t dist = manhattan_dist(points[i].x, medoids[j]);
            if (dist < min_dist) {
                min_dist = dist;
                closest_medoid = j;
            }
        }

        points[i].cluster_id = closest_medoid;
    }
}

uint64_t compute_cost(Point* points, int num_points, uint64_t* medoids, int k) {
    uint64_t total_cost = 0;

    // Calculate total distance cost for all points
    for (int i = 0; i < num_points; i++) {
        uint64_t min_dist = manhattan_dist(points[i].x, medoids[0]);
        for (int j = 1; j < k; j++) {
            uint64_t dist = manhattan_dist(points[i].x, medoids[j]);
            if (dist < min_dist) {
                min_dist = dist;
            }
        }
        total_cost += min_dist;
    }

    return total_cost;
}

void k_medoids(Point* points, int num_points, int k, int max_iter, uint64_t* medoids) {
    // Initialize medoids randomly
    srand(time(NULL));
    for (int i = 0; i < k; i++) {
        int random_idx = rand() % num_points;
        medoids[i] = points[random_idx].x;
    }

    uint64_t* old_medoids = (uint64_t*)malloc(k * sizeof(uint64_t));
    for (int i = 0; i < k; i++) {
        old_medoids[i] = 0;
    }

    for (int iteration = 0; iteration < max_iter; iteration++) {
        // Assign points to medoids
        assign_points_to_medoids(points, num_points, medoids, k);

        

        // Check for convergence (if medoids don't change)
        int converged = 1;
        for (int i = 0; i < k; i++) {
            if (medoids[i] != old_medoids[i]) {
                converged = 0;
                break;
            }
        }

        if (converged) {
            printf("Converged after %d iterations.\n", iteration + 1);
            break;
        }

        // Copy current medoids to old_medoids
        for (int i = 0; i < k; i++) {
            old_medoids[i] = medoids[i];
        }
    }

    free(old_medoids);
}
