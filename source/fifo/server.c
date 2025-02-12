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

void communicate(int readStream, int writeStream,
								 struct Arguments *args,
								 struct sigaction *signal_action) {
	
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	void *readBuffer = malloc(bufferSize);

	struct Benchmarks bench;
	bool firstRead = true;
	bench_t endTime = 0;

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(2 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = readStream;
	pfds[1].fd = writeStream;
	pfds[0].events = POLLIN;
 	pfds[1].events = POLLOUT;

	// Check only for reads on the first poll
	int sizeToPoll = 1;
	
	ssize_t outstandingBytes = 0;
	ssize_t maxOutstandingBytes = reqSize * args->rate;
	ssize_t maxBytes = reqSize * args->count;
	ssize_t totalBytesRead = 0;
	ssize_t totalBytesWritten = 0;

	while((totalBytesWritten < maxBytes) && quit == 0) {
		// Need to log time before poll
		endTime = now();
		
		if(!checkPoll(pfds, sizeToPoll, 1000)) {
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

		size_t toRead = maxOutstandingBytes;
		size_t toWrite = outstandingBytes;


		if (outstandingBytes > maxOutstandingBytes) {
			toRead = outstandingBytes;
		}

		if (canRead && (quit == 0)) {
			readInfo = read_from_pipe(readBuffer, readStream, reqSize, toRead);
			totalBytesRead += readInfo.totalBytes;
			outstandingBytes += readInfo.totalBytes;

			if (readInfo.totalBytes != 0) {
				toWrite += readInfo.totalBytes;
				
				if (firstRead) {
					setup_benchmarks(&bench);
					sizeToPoll = 2;
					firstRead = false;
				}
			}
		}

		if (canWrite && (quit == 0)) {
			writeInfo = write_to_pipe(readBuffer, writeStream, reqSize, toWrite);
			totalBytesWritten += writeInfo.totalBytes;
			outstandingBytes -= writeInfo.totalBytes;
		}
	}
	// Client has written everything that they can thus far, tell them to stop
	cleanup(readStream);
	cleanup(writeStream);

	ssize_t numReqs = totalBytesWritten / reqSize;
	evaluateServer(&bench, numReqs, endTime);
}

int open_fifo(const char* path, int flag, struct sigaction *signal_action) {
	int fd = open(path, flag);
	if (fd == -1) {
		printf("errno %d\n", errno);
		return -1;
	}

  	int result = fcntl(fd, F_SETPIPE_SZ, 1048576);
	if (result == -1) {
		printf("errno %d\n", errno);
		return -1;
  	}

	return fd;
}

int main(int argc, char* argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty

	// Set the CPU affinity to core 0 (first core)
	CPU_SET(5, &cpuset);
	setvbuf(stdout, NULL, _IONBF, 0);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		return EXIT_FAILURE;
	}

	// The file pointers we will associate with the FIFO
	int clientWriteStream;
	int clientReadStream;

	// For server/client signals
	struct sigaction signal_action;
	setup_server_signals(&signal_action);

	struct Arguments args;
	parse_arguments(&args, argc, argv);

	clientWriteStream = open_fifo(RESPONSE, O_WRONLY, &signal_action);
	if (clientWriteStream == -1) {
		fflush(stdout);
		return EXIT_FAILURE;
	}

	clientReadStream = open_fifo(REQUEST, O_RDONLY, &signal_action);
	if (clientReadStream == -1) {
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
