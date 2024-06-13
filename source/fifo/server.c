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

#define READ_FIFO_PATH "/tmp/ipc_bench_fifo_write"
#define WRITE_FIFO_PATH "/tmp/ipc_bench_fifo_read"

void cleanup(int stream, void *buffer) {
	free(buffer);
	close(stream);
}

bool checkPoll(struct pollfd* pfds, int timeToWait) {
	int ret = poll(pfds, 2, timeToWait);
	if (ret == -1) {
		throw("error");
	}

	return true;
}

void communicate(int readStream, int writeStream,
								 struct Arguments *args,
								 struct sigaction *signal_action) {
	
	void *readBuffer = malloc(args->size * args->rate);

	size_t reqsServed = 0;

	struct Benchmarks bench;
	setup_benchmarks(&bench);
	wait_for_signal(signal_action);
	
	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(2 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = readStream;
 	pfds[0].events = POLLIN;

	// See if we can write anything
  	pfds[1].fd = writeStream;
  	pfds[1].events = POLLOUT;

	while(reqsServed < args->count) {
		// Read and send as much as possible
		if(checkPoll(pfds, 0) && (pfds[0].revents && POLLIN) && (pfds[0].revents && POLLOUT)) {
			read_from_pipe(readBuffer, readStream, sizeof(bench_t) + args->size);
			write_to_pipe(readBuffer, writeStream, sizeof(bench_t) + args->size);
			reqsServed++;
		}
	}
	
	evaluateServer(&bench, reqsServed);
	cleanup(readStream, readBuffer);
	close(writeStream);
}

int open_fifo(const char* path, struct Arguments *args) {
	int fd;
	
	fd = mkfifo(path, 0666);
	if (fd > 0) {
		throw("Error creating FIFO");
	}

	fd = open(path, O_RDWR);

	// Use fcntl to set the pipe size on the opened file descriptor
  	int result = fcntl(fd, F_SETFL, O_NONBLOCK, F_SETPIPE_SZ, 102400);
	if (result == -1) {
		perror("fcntl");
		exit(1);
  	}	

	return fd;
}

int main(int argc, char* argv[]) {
	// cpu_set_t cpuset;  // Set of CPUs
  	// CPU_ZERO(&cpuset);  // Initialize the set to empty

	// // Set the CPU affinity to core 1 (second core)
	// CPU_SET(0, &cpuset);

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

	setup_server_signals(&signal_action);

	clientReadStream = open_fifo(READ_FIFO_PATH, &args);
	clientWriteStream = open_fifo(WRITE_FIFO_PATH, &args);

	// Tell client both pipes have been set up
	notify_client();
	communicate(clientReadStream, clientWriteStream, &args, &signal_action);

	return EXIT_SUCCESS;
}
