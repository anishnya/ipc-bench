#ifndef IPC_BENCH_PROCESS_H
#define IPC_BENCH_PROCESS_H

#include <stdatomic.h>
#include <string.h> // memcpy
#include <stdlib.h> //realloc
#include "common/benchmarks.h"

void rate_wait(atomic_char* guard);
void rate_full_notify(atomic_char* guard);
void rate_empty_notify(atomic_char* guard);

bench_t readTimeFromBuff(void *buffer);
void writeTimeToBuff(void *buffer);

#endif /* IPC_BENCH_SIGNALS_H */