add_library(ioblazerC ioblazer.c ioblazerbup.c)

add_library(ioblazer INTERFACE)
target_link_libraries(ioblazer INTERFACE ioblazerC)