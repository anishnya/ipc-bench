#ifndef FIFO_HELPER_H
#define FIFO_HELPER_H
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include "common/common.h"
#include "common/utility.h"
#include <linux/kernel-page-flags.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

#define READ_SOCKET_PATH "/tmp/ipc_bench_socket_READ"
#define WRITE_SOCKET_PATH "/tmp/ipc_bench_socket_WRITE"

struct socketMetaData {
    size_t wholeReqs;
    size_t spareBytes;
    size_t totalBytes;
    int error;
};

struct socketMetaData read_from_socket(void *buffer, int stream, size_t chunk, size_t numBytesToRead, bool nonBlock);
struct socketMetaData write_to_socket(void *buffer, int stream, size_t chunk, size_t numBytesToWrite, bool nonBlock);

#endif