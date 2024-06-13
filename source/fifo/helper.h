#ifndef FIFO_HELPER_H
#define FIFO_HELPER_H
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <poll.h>
#include "common/common.h"
#include "common/utility.h"
#include <linux/kernel-page-flags.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

void write_to_pipe(void *buffer, int stream, size_t num_bytes);
void read_from_pipe(void *buffer, int stream, size_t num_bytes);
int maxPipeSize(struct Arguments *args);

#endif