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

volatile int quit = 0;

static void quit_signal_handler(int signal_number) {
    quit = 1;
}

void cleanup(int connection) {
	if (close(connection) == -1) {
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

void communicate(int connection, struct Arguments* args) {
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	void *readBuffer = malloc(bufferSize);

	struct Benchmarks bench;
	bool firstRead = true;
	bench_t endTime = 0;

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(1 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = connection;
	pfds[0].events = POLLIN;

	ssize_t outstandingBytes = 0;
	ssize_t maxOutstandingBytes = reqSize * args->rate;
	ssize_t maxBytes = reqSize * args->count;
	ssize_t totalBytesRead = 0;
	ssize_t totalBytesWritten = 0;

	while(totalBytesWritten < maxBytes && quit == 0) {
		// Need to log time before a poll
		endTime = now();
		
		if (!checkPoll(pfds, 1, 1000)) {
			break;
		}

		if (quit != 0) {
			break;
		}
		
		int canRead = (pfds[0].revents & POLLIN);
		int canWrite = (pfds[0].revents & POLLOUT);
		struct socketMetaData readInfo = {0, 0, 0, 0};
		struct socketMetaData writeInfo = {0, 0, 0, 0};
		size_t toRead = maxOutstandingBytes;
		size_t toWrite = outstandingBytes;

		if (outstandingBytes > maxOutstandingBytes) {
			toRead = outstandingBytes;
		}

		if (canRead && (quit == 0)) {
			readInfo = read_from_socket(readBuffer, connection, reqSize, toRead, false);
			totalBytesRead += readInfo.totalBytes;
			outstandingBytes += readInfo.totalBytes;
			if (readInfo.totalBytes != 0) {
				toWrite += readInfo.totalBytes;
				// When we can read, only then do we care that we can write

				if (firstRead) {
					setup_benchmarks(&bench);
					pfds[0].events = POLLIN | POLLOUT;
					firstRead = false;
				}
			}
		}

		if (canWrite && (quit == 0)) {
			writeInfo = write_to_socket(readBuffer, connection, reqSize, toWrite, false);
			totalBytesWritten += writeInfo.totalBytes;
			outstandingBytes -= writeInfo.totalBytes;
		}
	}
	
	ssize_t numReqs = totalBytesWritten / reqSize;
	evaluateServer(&bench, numReqs, endTime);
	cleanup(connection);
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

	if (connection == -1) {
		return -1;
	}

	set_socket_both_buffer_sizes(connection);
	close(socket_descriptor);

	return connection;
}

int main(int argc, char* argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty

	// Set the CPU affinity to core 0 (first core)
	CPU_SET(5, &cpuset);
	setvbuf(stdout, NULL, _IONBF, 0);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		perror("sched_setaffinity");
		exit(1);
	}
	// File descriptor and connection for the server sockets
	int socketDescriptor = -1;
	int connection = -1;
	
	// Fifo signaling
	int socketFifo = -1;
	void *buffer = malloc(1);

	// For command-line arguments
	struct Arguments args;
	parse_arguments(&args, argc, argv);

	struct sigaction signal_action;
	setup_server_signals(&signal_action);


	// must be in order
	while (socketDescriptor == -1) {
		socketDescriptor = create_socket(READ_SOCKET_PATH);
	}

	// Tell client socket is ready
	socketFifo = open_fifo(SOCKET_FIFO_PATH, O_RDWR);
	if(write(socketFifo, buffer, 1) <= 0) {
		throw("bad write");
	}

	while (connection == -1) {
		connection = accept_connection(socketDescriptor);
	}

	struct sigaction quit_signal_action;
    quit_signal_action.sa_handler = quit_signal_handler;
    
	if (sigaction(SIGINT, &quit_signal_action, NULL) != 0) {
        return EXIT_FAILURE;
    }

	signal(SIGPIPE, quit_signal_handler);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);

	communicate(connection, &args);

	return EXIT_SUCCESS;
}
