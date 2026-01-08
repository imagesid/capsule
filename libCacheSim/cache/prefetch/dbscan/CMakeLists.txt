add_library(dbscanC dbscan.c)

add_library(dbscan INTERFACE)
target_link_libraries(dbscan INTERFACE dbscanC)