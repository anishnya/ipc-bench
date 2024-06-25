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

void communicate(int readStream, int writeStream,
								 struct Arguments *args,
								 struct sigaction *signal_action) {
	
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	void *readBuffer = malloc(bufferSize);

	struct Benchmarks bench;
	setup_benchmarks(&bench);
	wait_for_signal(signal_action);

	ssize_t outstandingBytes = 0;
	ssize_t maxOutstandingBytes = reqSize * args->rate;
	ssize_t maxBytes = reqSize * args->count;
	ssize_t totalBytesRead = 0;
	ssize_t totalBytesWritten = 0;

	while(totalBytesWritten < maxBytes) {
		struct pipeMetaData readInfo;
		struct pipeMetaData writeInfo;
		size_t toRead = maxOutstandingBytes;
		size_t toWrite = outstandingBytes;

		if (outstandingBytes > maxOutstandingBytes) {
			toRead = outstandingBytes;
		}

		readInfo = read_from_pipe(readBuffer, readStream, reqSize, toRead);
		totalBytesRead += readInfo.totalBytes;
		outstandingBytes += readInfo.totalBytes;

		if (readInfo.totalBytes != 0) {
			toWrite += readInfo.totalBytes;
		}

		writeInfo = write_to_pipe(readBuffer, writeStream, reqSize, toWrite);
		totalBytesWritten += writeInfo.totalBytes;
		outstandingBytes -= writeInfo.totalBytes;
	}
	
	evaluateServer(&bench, args->count);
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
  	int result = fcntl(fd, F_SETPIPE_SZ, 102400);
	if (result == -1) {
		perror("fcntl");
		exit(1);
  	}

	// result = fcntl(fd, F_SETFL);
	// if (result == -1) {
	// 	perror("fcntl");
	// 	exit(1);
  	// }

	return fd;
}

int main(int argc, char* argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty

	// Set the CPU affinity to core 0 (first core)
	CPU_SET(0, &cpuset);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		perror("sched_setaffinity");
		exit(1);
	}

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
