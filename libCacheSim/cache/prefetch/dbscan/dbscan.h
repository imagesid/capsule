#ifndef DBSCAN_H
#define DBSCAN_H

#include <stdint.h>

#define UNCLASSIFIED -1
#define NOISE 0
#define SUCCESS 1
#define FAILURE 0
#include "../type.h"

// typedef struct {
//     uint64_t x;
//     int cluster_id;
// } Point;

double distance(Point a, Point b);
void dbscan(Point *points, int num_points, double eps, int min_pts);
int get_cluster_id(Point *points, int num_points, uint64_t point_value);
void print_points_in_cluster(Point *points, int num_points, int cluster_id);

#endif // DBSCAN_H
