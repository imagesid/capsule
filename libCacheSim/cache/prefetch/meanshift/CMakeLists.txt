add_library(meanshiftC mean_shift.c)

add_library(meanshift INTERFACE)
target_link_libraries(meanshift INTERFACE meanshiftC)