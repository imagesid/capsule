add_library(kmeansC kmeans.c)

add_library(kmeans INTERFACE)
target_link_libraries(kmeans INTERFACE kmeansC)