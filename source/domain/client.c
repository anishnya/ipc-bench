#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "common/common.h"
#include "common/sockets.h"
#include "domain/helper.h"
#include <sched.h>


void cleanup(int connection, void* buffer) {
	close(connection);
	free(buffer);
}

void resetPollStruct(struct pollfd* pfds, int readFD, int writeFD, short readEvent, short writeEvent) {
	pfds[0].fd = readFD;
	pfds[1].fd = writeFD;
	pfds[0].events = readEvent;
	pfds[1].events = writeEvent;
	pfds[0].revents = 0;
	pfds[1].revents = 0;
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
		times[i] = ((time - times[i]) / 1000);
	}

	return;	
}

void communicate(int connection, struct Arguments* args) {
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	
	void *readBuffer = malloc(bufferSize);
	void *writeBuffer = malloc(bufferSize);
	bench_t *times = malloc(sizeof(bench_t) * args->count); 

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(1 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = connection;
	pfds[0].events = POLLIN | POLLOUT;

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

	while(totalBytesRead < maxBytes) {
		checkPoll(pfds, 1, wait);
		int canRead = (pfds[0].revents & POLLIN);
		int canWrite = (pfds[0].revents & POLLOUT);
		struct socketMetaData readInfo = {0, 0, 0, 0};
		struct socketMetaData writeInfo = {0, 0, 0, 0};
		size_t toUpdateRead = 0;

		// Can Read
		if (canRead) {
			readInfo = read_from_socket(readBuffer, connection, reqSize, outstandingBytes, false);
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
			writeInfo = write_to_socket(writeBuffer, connection, reqSize, (maxOutstandingBytes - outstandingBytes), false);
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

	// tell server we are done reading data
	client_once(NOTIFY);
	const bench_t elaspedTime = now() - bench.total_start;
	printf("Total duration:     %lld\n", elaspedTime / 1000);
	evaluateClient(times, args);
	// cleanup(connection, buffer);
}

void setup_socket(int connection, char* socketPath) {
	int return_code;

	// The main datastructure for a UNIX-domain socket.
	// It only has two members:
	// 1. sun_family: The family of the socket. Should be AF_UNIX
	//                for UNIX-domain sockets.
	// 2. sun_path: Noting that a UNIX-domain socket ist just a
	//              file in the file-system, it also has a path.
	//              This may be any path the program has permission
	//              to create, read and write files in. The maximum
	//              size of such a path is 108 bytes.

	struct sockaddr_un address;

	set_socket_both_buffer_sizes(connection);

	// if (set_io_flag(connection, O_NONBLOCK) == -1) {
	// 	throw("Error setting socket to non-blocking on client-side");
	// }

	// Set the family of the address struct
	address.sun_family = AF_UNIX;
	// Copy in the path
	strcpy(address.sun_path, socketPath);

	// Connect the socket to an address.
	// Arguments:
	// 1. The socket file-descriptor.
	// 2. A sockaddr struct describing the socket address to connect to.
	// 3. The length of the struct, as computed by the SUN_LEN macro.
	// clang-format off
	// Blocks until the connection is accepted by the other end.
	return_code = connect(
		connection,
		(struct sockaddr*)&address,
		SUN_LEN(&address)
	);
	// clang-format on

	if (return_code == -1) {
		throw("Error connecting to server");
	}
}

int create_connection(char* socketPath) {
	// The connection socket (file descriptor) that we will return
	int connection;

	// Wait until the server is listening on the socket
	client_once(WAIT);

	// Get a new socket from the OS
	// Arguments:
	// 1. The family of the socket (AF_UNIX for UNIX-domain sockets)
	// 2. The socket type, either stream-oriented (TCP) or
	//    datagram-oriented (UDP)
	// 3. The protocol for the given socket type. By passing 0, the
	//    OS will pick the right protocol for the job (TCP/UDP)
	connection = socket(AF_UNIX, SOCK_STREAM, 0);

	if (connection == -1) {
		throw("Error opening socket on client-side");
	}

	setup_socket(connection, socketPath);

	return connection;
}

int main(int argc, char* argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty
	CPU_SET(1, &cpuset);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		perror("sched_setaffinity");
		exit(1);
	}
	
	// File descriptor for the socket over which
	// the communciation will happen with the client
	int connection;

	// For command-line arguments
	struct Arguments args;
	parse_arguments(&args, argc, argv);

	connection = create_connection(READ_SOCKET_PATH);
	communicate(connection, &args);

	return EXIT_SUCCESS;
}
