#define _GNU_SOURCE
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "common/common.h"
#include "fifo/helper.h"
#include "common/rateGuard.h"
#include <sys/select.h>
#include <fcntl.h>
#include <sched.h>

#define READ_FIFO_PATH "/tmp/ipc_bench_fifo_read"
#define WRITE_FIFO_PATH "/tmp/ipc_bench_fifo_write"

atomic_llong outstandingReqs = 0;

void cleanup(int stream, void *buffer) {
	free(buffer);
	close(stream);
}

bool checkPoll(struct pollfd* pfds, int timeToWait) {	
	int ret = poll(pfds, 1, timeToWait);
	if (ret == -1) {
		throw("error");
	}

	return true;
}

typedef struct {
	void *readBuffer;
	int readStream;
	struct Arguments *args;
	struct Benchmarks *bench;
} readArgs;

void *read_reqs(void *arg) {
	readArgs *args = (readArgs *)arg;

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(1 * sizeof(struct pollfd));
	pfds[0].fd = args->readStream;
 	pfds[0].events = POLLIN;
	size_t numReqs = 0;
	int waitTime = 0;

	while(numReqs < args->args->count) {
		checkPoll(pfds, waitTime);
		if ((pfds[0].revents && POLLIN)) {
			read_from_pipe(args->readBuffer, args->readStream, sizeof(bench_t) + args->args->size);
			atomic_fetch_sub_explicit(&outstandingReqs, 1, memory_order_relaxed);
			numReqs++;
		}
	}

	args->bench->sum = now() - args->bench->total_start;

    return NULL;
}

void communicate(int readStream, int writeStream,
								 struct Arguments *args,
								 struct sigaction *signal_action) {
	
	void *readBuffer = malloc(args->size + sizeof(bench_t));
	memset(readBuffer, 1 << 6, args->size + sizeof(bench_t));
	
	void *writeBuffer = malloc(args->size + sizeof(bench_t));
	memset(writeBuffer, 1 << 6, args->size + sizeof(bench_t));
	
	struct Benchmarks bench;
	setup_benchmarks(&bench);

	pthread_t thread;

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(1 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = writeStream;
 	pfds[0].events = POLLOUT;

	notify_server(signal_action);

	readArgs toSend;
	toSend.readBuffer = readBuffer;
	toSend.readStream = readStream;
	toSend.args = args;
	toSend.bench = &bench;
	
	int writeWait = 0;
	
	size_t numReqs = 0;
	pthread_create(&thread, NULL, read_reqs, &toSend);
	
	while(numReqs < args->count) {
		// Write as much as I can before a read
		if(checkPoll(pfds, writeWait) && (pfds[0].revents && POLLOUT) && atomic_load_explicit(&outstandingReqs, memory_order_acquire) < args->rate) {
			write_to_pipe(writeBuffer, writeStream, sizeof(bench_t) + args->size);
			atomic_fetch_add_explicit(&outstandingReqs, 1, memory_order_acq_rel);
			numReqs++;
		}
	}
	
	pthread_join(thread, NULL);
	evaluate(&bench, args);
	cleanup(writeStream, writeBuffer);
	cleanup(readStream, readBuffer);
}

int open_fifo(const char* path) {
	int fd = open(path, O_RDWR); 
	if (fd == -1) {
		throw("Error opening stream to FIFO on client-side");
	}

	// Use fcntl to set the pipe size on the opened file descriptor
  	int result = fcntl(fd, F_SETFL, O_NONBLOCK, F_SETPIPE_SZ, 102400);
	if (result == -1) {
		perror("fcntl");
		exit(1);
  	}	


	return fd;
}

int main(int argc, char *argv[]) {
	// cpu_set_t cpuset;  // Set of CPUs
  	// CPU_ZERO(&cpuset);  // Initialize the set to empty

	// // Set the CPU affinity to core 1 (second core)
	// CPU_SET(1, &cpuset);

	// if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
	// 	perror("sched_setaffinity");
	// 	exit(1);
	// }
	// The file pointers we will associate with the FIFO
	int clientWriteStream;
	int clientReadStream;

	// For server/client signals
	struct sigaction signal_action;

	struct Arguments args;
	parse_arguments(&args, argc, argv);

	setup_client_signals(&signal_action);
	// Wait for server to set up stuff
	wait_for_signal(&signal_action);
	clientReadStream = open_fifo(READ_FIFO_PATH);
	clientWriteStream = open_fifo(WRITE_FIFO_PATH);
	communicate(clientReadStream, clientWriteStream, &args, &signal_action);

	return EXIT_SUCCESS;
}
