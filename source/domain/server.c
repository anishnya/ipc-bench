#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <sched.h>

#include "common/common.h"
#include "common/sockets.h"
#include "domain/helper.h"

void cleanup(int connection, char *socketPath, void* buffer) {
	close(connection);
	free(buffer);
	if (remove(socketPath) == -1) {
		throw("Error removing domain socket");
	}
}

void communicate(int connection, struct Arguments* args) {
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	void *readBuffer = malloc(bufferSize);

	struct Benchmarks bench;
	setup_benchmarks(&bench);

	ssize_t outstandingBytes = 0;
	ssize_t maxOutstandingBytes = reqSize * args->rate;
	ssize_t maxBytes = reqSize * args->count;
	ssize_t totalBytesRead = 0;
	ssize_t totalBytesWritten = 0;

	while(totalBytesWritten < maxBytes) {
		struct socketMetaData readInfo = {0, 0, 0, 0};
		struct socketMetaData writeInfo = {0, 0, 0, 0};
		size_t toRead = maxOutstandingBytes;
		size_t toWrite = outstandingBytes;

		if (outstandingBytes > maxOutstandingBytes) {
			toRead = outstandingBytes;
		}

		readInfo = read_from_socket(readBuffer, connection, reqSize, toRead, false);
		totalBytesRead += readInfo.totalBytes;
		outstandingBytes += readInfo.totalBytes;

		if (readInfo.totalBytes != 0) {
			toWrite += readInfo.totalBytes;
		}

		writeInfo = write_to_socket(readBuffer, connection, reqSize, toWrite, false);
		totalBytesWritten += writeInfo.totalBytes;
		outstandingBytes -= writeInfo.totalBytes;
	}
	
	// before closing connection, wait for client to finish
	server_once(WAIT);
	evaluateServer(&bench, args->count);
}

void setup_socket(int socket_descriptor, char* socketPath) {
	int return_code;

	// The main datastructure for a UNIX-domain socket.
	// It only has two members:
	// 1. sun_family: The family of the socket. Should be AF_UNIX
	//                for UNIX-domain sockets (AF_LOCAL is the same,
	//                but AF_UNIX is POSIX).
	// 2. sun_path: Noting that a UNIX-domain socket ist just a
	//              file in the file-system, it also has a path.
	//              This may be any path the program has permission
	//              to create, read and write files in. The maximum
	//              size of such a path is 108 bytes.
	struct sockaddr_un address;

	// Set the family of the address struct
	address.sun_family = AF_UNIX;
	// Copy in the path
	strcpy(address.sun_path, socketPath);
	// Remove the socket if it already exists
	remove(address.sun_path);

	// Bind the socket to an address.
	// Arguments:
	// 1. The socket file-descriptor.
	// 2. A sockaddr struct, which we get by casting our address struct.
	// 3. The length of the struct, as computed by the SUN_LEN macro.
	// clang-format off
	return_code = bind(
		socket_descriptor,
		(struct sockaddr*)&address,
		SUN_LEN(&address)
	);
	// clang-format on

	if (return_code == -1) {
		throw("Error binding socket to address");
	}

	// Enable listening on this socket
	return_code = listen(socket_descriptor, 10);

	if (return_code == -1) {
		throw("Could not start listening on socket");
	}
}

int create_socket(char* socketPath) {
	// File descriptor for the socket
	int socket_descriptor;

	// Get a new socket from the OS
	// Arguments:
	// 1. The family of the socket (AF_UNIX for UNIX-domain sockets)
	// 2. The socket type, either stream-oriented (TCP) or
	//    datagram-oriented (UDP)
	// 3. The protocol for the given socket type. By passing 0, the
	//    OS will pick the right protocol for the job (TCP/UDP)
	socket_descriptor = socket(AF_UNIX, SOCK_STREAM, 0);

	if (socket_descriptor == -1) {
		throw("Error opening socket on server-side");
	}

	setup_socket(socket_descriptor, socketPath);
	
	// Notify the client that it can connect to the socket now
	server_once(NOTIFY);

	return socket_descriptor;
}

int accept_connection(int socket_descriptor) {
	struct sockaddr_un client;
	int connection;
	socklen_t length = sizeof client;

	// Start accepting connections on this socket and
	// receive a connection-specific socket for any
	// incoming socket
	// clang-format off
	connection = accept(
		socket_descriptor,
		(struct sockaddr*)&client,
		&length
	);

	// clang-format on

	if (connection == -1) {
		throw("Error accepting connection");
	}

	set_socket_both_buffer_sizes(connection);

	return connection;
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
	// File descriptor for the server sockets
	int socketDescriptor;

	// File descriptor for the socket over which
	// the communciation will happen with the client
	int connection;

	// For command-line arguments
	struct Arguments args;

	parse_arguments(&args, argc, argv);

	// must be in order
	socketDescriptor = create_socket(READ_SOCKET_PATH);
	connection = accept_connection(socketDescriptor);
	
	communicate(connection, &args);

	return EXIT_SUCCESS;
}
