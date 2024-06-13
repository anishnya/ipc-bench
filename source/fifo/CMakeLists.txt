###########################################################
## TARGETS
###########################################################

add_executable(fifo-client client.c helper.c)
add_executable(fifo-server server.c helper.c)
add_executable(fifo fifo.c)


###########################################################
## COMMON
###########################################################

target_link_libraries(fifo-client ipc-bench-common)
target_link_libraries(fifo-server ipc-bench-common)
target_link_libraries(fifo ipc-bench-common)
