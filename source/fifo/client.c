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

volatile int quit = 0;

static void quit_signal_handler(int signal_number) {
    quit = 1;
}

void cleanup(int stream) {
	if (close(stream) == -1) {
		return;
	}

	return;
}

bool checkPoll(struct pollfd* pfds, int numPolled, int timeToWait) {
	int ret = poll(pfds, numPolled, timeToWait);
	if (ret == -1) {
		return false;
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
		times[i] = ((time - times[i]));
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

	while((totalBytesRead < maxBytes) && (quit == 0)) {
		if (!checkPoll(pfds, 2, wait)) {
			break;
		}

		// Kill after poll
		if (quit != 0) {
			break;
		}

		int canRead = pfds[0].revents & POLLIN;
		int canWrite = pfds[1].revents & POLLOUT; 
		struct pipeMetaData readInfo = {0, 0, 0};
		struct pipeMetaData writeInfo = {0, 0, 0};
		size_t toUpdateRead = 0;

		// If you can read
		if (canRead && (quit == 0)) {
			readInfo = read_from_pipe(readBuffer, readStream, reqSize, outstandingBytes);
			outstandingBytes -= readInfo.totalBytes;
			totalBytesRead += readInfo.totalBytes;

			if (readInfo.wholeReqs > 0) {
				outstandingReqs -= readInfo.wholeReqs;
				toUpdateRead += readInfo.wholeReqs;
			}

			updateReadWriteTimes(now(), times, args->count, numReqsRead, toUpdateRead);
			numReqsRead += toUpdateRead;
		}

		if (outstandingReqs < maxOutstandingReqs) {
			updateWriteTimes(now(), times, args->count, numReqsSent, (maxOutstandingReqs - outstandingReqs));
			numReqsSent += (maxOutstandingReqs - outstandingReqs);
		}

		// Can write
		if (canWrite && (outstandingBytes < maxOutstandingBytes) && (quit == 0)) {
			writeInfo = write_to_pipe(writeBuffer, writeStream, reqSize, (maxOutstandingBytes - outstandingBytes));
			outstandingBytes += writeInfo.totalBytes;
			totalBytesWritten += writeInfo.totalBytes;
			wait = 1000;

			if (writeInfo.wholeReqs > 0) {
				outstandingReqs += writeInfo.wholeReqs;
			}
		}
	}
	// Needed upon a kill
	args->count = (numReqsRead > args->count) ? args->count : numReqsRead;
	evaluateClient(times, args);
	cleanup(readStream);
	cleanup(writeStream);
}

int open_fifo(const char* path, int flag, struct sigaction *signal_action) {
	int fd = open(path, flag); 
	if (fd == -1) {
		printf("errno %d\n", errno);
		return -1;
	}

	int result = fcntl(fd, F_SETPIPE_SZ, 102400);
	if (result == -1) {
		printf("errno %d\n", errno);
		return -1;
  	}

	return fd;
}

int main(int argc, char *argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty
	
	CPU_SET(3, &cpuset);
	setvbuf(stdout, NULL, _IONBF, 0);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		return EXIT_FAILURE;
	}

	// The file pointers we will associate with the FIFO
	int clientWriteStream;
	int clientReadStream;

	struct sigaction signal_action;
	setup_client_signals(&signal_action);

	struct Arguments args;
	parse_arguments(&args, argc, argv);	

	clientReadStream = open_fifo(RESPONSE, O_RDONLY, &signal_action);
	if (clientReadStream == -1) {
		fflush(stdout);
		printf("%d\n", clientReadStream);
		return EXIT_FAILURE;
	}

	clientWriteStream = open_fifo(REQUEST, O_WRONLY, &signal_action);
	if (clientWriteStream == -1) {
		fflush(stdout);
		return EXIT_FAILURE;
	}

	struct sigaction quit_signal_action;
    quit_signal_action.sa_handler = quit_signal_handler;
    
	if (sigaction(SIGINT, &quit_signal_action, NULL) != 0) {
        return EXIT_FAILURE;
    }

	signal(SIGPIPE, quit_signal_handler);
	signal(SIGCHLD, SIG_IGN);
  	signal(SIGTTIN, SIG_IGN);

	communicate(clientReadStream, clientWriteStream, &args, &signal_action);

	return EXIT_SUCCESS;
}
