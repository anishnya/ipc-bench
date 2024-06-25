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


void cleanup(int stream, void *buffer) {
	free(buffer);
	close(stream);
}

bool checkPoll(struct pollfd* pfds, int numPolled, int timeToWait) {	
	int ret = poll(pfds, numPolled, timeToWait);
	if (ret == -1) {
		throw("error");
	}

	return true;
}

void updateWriteTimes(bench_t time, bench_t *times, size_t totalTimes, size_t startIndex, size_t numToUpdate) {
	size_t end = startIndex + numToUpdate; 
	if (startIndex > totalTimes) {
		return;
	}
	
	if (startIndex + numToUpdate > totalTimes) {
		end = totalTimes;
	}

	for (size_t i = startIndex; i < end; ++i) {
		times[i] = time;
	}

	return;
}

void updateReadWriteTimes(bench_t time, bench_t *times, size_t totalTimes, size_t startIndex, size_t numToUpdate) {
	size_t end = startIndex + numToUpdate; 
	if (startIndex > totalTimes) {
		return;
	}
	
	if (startIndex + numToUpdate > totalTimes) {
		end = totalTimes;
	}
	
	for (size_t i = startIndex; i < end; ++i) {
		// Prevent overflow
		times[i] = ((time - times[i]) / 1000);
	}

	return;	
}


void communicate(int readStream, int writeStream,
								 struct Arguments *args,
								 struct sigaction *signal_action) {
	
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	
	void *readBuffer = malloc(bufferSize);
	void *writeBuffer = malloc(bufferSize);
	bench_t *times = malloc(sizeof(bench_t) * args->count); 

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(2 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = readStream;
	pfds[1].fd = writeStream;
	pfds[0].events = POLLIN;
 	pfds[1].events = POLLOUT;

	struct Benchmarks bench;
	setup_benchmarks(&bench);

	notify_server(signal_action);
	
	int wait = 0;
	size_t outstandingBytes = 0;
	size_t maxBytes = reqSize * args->count;
	size_t maxOutstandingBytes = reqSize * args->rate;
	ssize_t totalBytesRead = 0;
	ssize_t totalBytesWritten = 0;

	size_t maxOutstandingReqs = args->rate;
	size_t numReqsRead = 0;
	size_t numReqsSent = 0;
	size_t outstandingReqs = 0;

	bench.total_start = now();

	while(totalBytesRead < maxBytes) {
		checkPoll(pfds, 2, wait);
		int canRead = pfds[0].revents & POLLIN;
		int canWrite = pfds[1].revents & POLLOUT; 
		struct pipeMetaData readInfo = {0, 0, 0};
		struct pipeMetaData writeInfo = {0, 0, 0};
		size_t toUpdateRead = 0;

		// If you can read
		if (canRead) {
			readInfo = read_from_pipe(readBuffer, readStream, reqSize, outstandingBytes);
			outstandingBytes -= readInfo.totalBytes;
			totalBytesRead += readInfo.totalBytes;

			if (readInfo.wholeReqs > 0) {
				outstandingReqs -= readInfo.wholeReqs;
				toUpdateRead += readInfo.wholeReqs;
			}

			if (readInfo.spareBytes > 0 && ((totalBytesRead % reqSize) == 0)) {
				outstandingReqs -= 1;
				toUpdateRead += 1;
			}

			updateReadWriteTimes(now(), times, args->count, numReqsRead, toUpdateRead);
			numReqsRead += toUpdateRead;
		}

		if (outstandingReqs < maxOutstandingReqs) {
			updateWriteTimes(now(), times, args->count, numReqsSent, (maxOutstandingReqs - outstandingReqs));
			numReqsSent += (maxOutstandingReqs - outstandingReqs);
		}

		// Can write
		if (canWrite && outstandingBytes < maxOutstandingBytes) {
			writeInfo = write_to_pipe(writeBuffer, writeStream, reqSize, (maxOutstandingBytes - outstandingBytes));
			outstandingBytes += writeInfo.totalBytes;
			totalBytesWritten += writeInfo.totalBytes;
			wait = -1;

			if (writeInfo.wholeReqs > 0) {
				outstandingReqs += writeInfo.wholeReqs;
			}

			if (writeInfo.spareBytes > 0 && ((totalBytesWritten % reqSize) == 0)) {
				outstandingReqs += 1;
			}
		}
	}
	
	const bench_t elaspedTime = now() - bench.total_start;
	printf("Total duration:     %lld\n", elaspedTime / 1000);
	evaluateClient(times, args);
	cleanup(writeStream, writeBuffer);
	cleanup(readStream, readBuffer);
}

int open_fifo(const char* path) {
	int fd = open(path, O_RDWR); 
	if (fd == -1) {
		throw("Error opening stream to FIFO on client-side");
	}

	// Use fcntl to set the pipe size on the opened file descriptor
  	int result = fcntl(fd, F_SETPIPE_SZ, 102400);
	if (result == -1) {
		perror("fcntl");
		exit(1);
  	}

	// result = fcntl(fd, F_SETFL, 0);
	// if (result == -1) {
	// 	perror("fcntl");
	// 	exit(1);
  	// }

	return fd;
}

int main(int argc, char *argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty
	CPU_SET(1, &cpuset);

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

	setup_client_signals(&signal_action);
	// Wait for server to set up stuff
	wait_for_signal(&signal_action);
	clientReadStream = open_fifo(READ_FIFO_PATH);
	clientWriteStream = open_fifo(WRITE_FIFO_PATH);
	communicate(clientReadStream, clientWriteStream, &args, &signal_action);

	return EXIT_SUCCESS;
}
