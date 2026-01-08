
file(GLOB dbscan_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/prefetch/dbscan/*.c
)
file(GLOB kmeans_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/prefetch/kmeans/*.c
)
file(GLOB meanshift_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/prefetch/meanshift/*.c
)
file(GLOB kmedoids_source
    ${PROJECT_SOURCE_DIR}/libCacheSim/cache/prefetch/kmedoids/*.c
)
add_library(prefetchC ${dbscan_source} ${kmeans_source} ${meanshift_source} ${kmedoids_source} Capsule.c Mithril.c OBL.c PG.c Cluster4.c PGCluster.c OBLCluster.c ../../../libCacheSim/bin/cachesim/debug.c)


add_library(prefetch INTERFACE)
target_link_libraries(prefetch INTERFACE prefetchC)


