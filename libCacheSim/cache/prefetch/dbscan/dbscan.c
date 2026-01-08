#include "dbscan.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double distance(Point a, Point b) {
    // if(b.x > 0){
    //     printf("ap %d - %d = %d \n", a.x, b.x, (double)a.x - (double)b.x);
    // }
    
    return fabs((double)a.x - (double)b.x);
}

int region_query(Point *points, int num_points, int point_idx, double eps, int *neighbors) {
    int num_neighbors = 0;
    for (int i = 0; i < num_points; i++) {
        if (distance(points[point_idx], points[i]) <= eps) {
            neighbors[num_neighbors++] = i;
        }
    }
    return num_neighbors;
}

int expand_cluster(Point *points, int num_points, int point_idx, int cluster_id, double eps, int min_pts) {
    int *neighbors = (int *)malloc(num_points * sizeof(int));
    int num_neighbors = region_query(points, num_points, point_idx, eps, neighbors);

    if (num_neighbors < min_pts) {
        points[point_idx].cluster_id = NOISE;
        free(neighbors);
        return FAILURE;
    }

    for (int i = 0; i < num_neighbors; i++) {
        points[neighbors[i]].cluster_id = cluster_id;
    }

    int i = 0;
    while (i < num_neighbors) {
        int current_point_idx = neighbors[i];
        int *result_neighbors = (int *)malloc(num_points * sizeof(int));
        int result_size = region_query(points, num_points, current_point_idx, eps, result_neighbors);
        
        if (result_size >= min_pts) {
            for (int j = 0; j < result_size; j++) {
                if (points[result_neighbors[j]].cluster_id == UNCLASSIFIED || points[result_neighbors[j]].cluster_id == NOISE) {
                    points[result_neighbors[j]].cluster_id = cluster_id;
                    neighbors[num_neighbors++] = result_neighbors[j];
                }
            }
        }
        free(result_neighbors);
        i++;
    }
    free(neighbors);
    return SUCCESS;
}

void dbscan(Point *points, int num_points, double eps, int min_pts) {
    int cluster_id = 1;
    for (int i = 0; i < num_points; i++) {
        if (points[i].cluster_id == UNCLASSIFIED) {
            // printf("ap %d\n", points[i].x);
            if (expand_cluster(points, num_points, i, cluster_id, eps, min_pts) == SUCCESS) {
                cluster_id++;
            }
        }
    }
}

int get_cluster_id(Point *points, int num_points, uint64_t point_value) {
    for (int i = 0; i < num_points; i++) {
        if (points[i].x == point_value) {
            return points[i].cluster_id;
        }
    }
    return UNCLASSIFIED; // Return UNCLASSIFIED if the point is not found
}

void print_points_in_cluster(Point *points, int num_points, int cluster_id) {
    printf("Points in cluster %d:\n", cluster_id);
    for (int i = 0; i < num_points; i++) {
        if (points[i].cluster_id == cluster_id) {
            // printf("Point (%llu)\n", points[i].x);
        }
    }
}
