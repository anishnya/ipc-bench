#ifndef IPC_BENCH_PROCESS_H
#define IPC_BENCH_PROCESS_H

#include <sys/shm.h>
#include <unistd.h>
#include "common/common.h"

char* create_mem(int size);
void shm_cleanup(int segment_id, char* shared_memory);

#endif /* IPC_BENCH_SIGNALS_H */