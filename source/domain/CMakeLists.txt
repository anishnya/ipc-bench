###########################################################
## TARGETS
###########################################################

add_executable(domain-client client.c helper.c)
add_executable(domain-server server.c helper.c)
add_executable(domain domain.c)

###########################################################
## COMMON
###########################################################

target_link_libraries(domain-client ipc-bench-common)
target_link_libraries(domain-server ipc-bench-common)
target_link_libraries(domain ipc-bench-common)
