#define _GNU_SOURCE
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tcp/helper.h"
#include "common/common.h"
#include "common/sockets.h"

int get_address(struct addrinfo *server_info) {
	struct addrinfo *iterator;
	int socket_descriptor;

	// For system call return values
	int return_code;

	// Iterate through the address linked-list until
	// we find one we can get a socket for
	for (iterator = server_info; iterator != NULL; iterator = iterator->ai_next) {
		// The call to socket() is the first step in establishing a socket
		// based communication. It takes the following arguments:
		// 1. The address family (PF_INET or PF_INET6)
		// 2. The socket type (SOCK_STREAM or SOCK_DGRAM)
		// 3. The protocol (TCP or UDP)
		// Note that all of these fields will have been already populated
		// by getaddrinfo. If the call succeeds, it returns a valid file descriptor.
		// clang-format off
		socket_descriptor = socket(
			iterator->ai_family,
			iterator->ai_socktype,
			iterator->ai_protocol
		 );
		// clang-format on

		if (socket_descriptor == -1) continue;

		// Once we have a socket, we can connect it to the server's socket.
		// Again, this information we get from the addrinfo struct
		// that was populated by getaddrinfo(). The arguments are:
		// 1. The socket file_descriptor from which to connect.
		// 2. The address to connect to (sockaddr_in struct)
		// 3. The size of this address structure.
		// clang-format off
		return_code = connect(
			socket_descriptor,
			iterator->ai_addr,
			iterator->ai_addrlen
		);
		// clang-format on

		// Could not connect to the server
		if (return_code == -1) {
			close(socket_descriptor);
			continue;
		}

		break;
	}

	// If we didn't actually find a valid address
	if (iterator == NULL) {
		throw("Error finding valid address!");
	}

	// Return the valid address info
	return socket_descriptor;
}

void cleanup(int descriptor) {
	close(descriptor);
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

void communicate(int descriptor, struct Arguments* args) {
	size_t reqSize = args->size;
	size_t bufferSize = reqSize * args->rate;
	
	void *readBuffer = malloc(bufferSize);
	void *writeBuffer = malloc(bufferSize);
	bench_t *times = malloc(sizeof(bench_t) * args->count); 

	struct pollfd *pfds;
	pfds = (struct pollfd*)malloc(1 * sizeof(struct pollfd));

	// See if we can read anything
	pfds[0].fd = descriptor;
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
			readInfo = read_from_socket(readBuffer, descriptor, reqSize, outstandingBytes, false);
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
			writeInfo = write_to_socket(writeBuffer, descriptor, reqSize, (maxOutstandingBytes - outstandingBytes), false);
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
	cleanup(descriptor);
}

void get_server_information(struct addrinfo **server_info) {
	// For system call return values
	int return_code;

	// We can supply some hints to the call to getaddrinfo
	// as to what socket family (domain) or what socket type
	// we want for the server address.
	struct addrinfo hints;

	// Fill the hints with zeros first
	memset(&hints, 0, sizeof hints);

	// We set to AF_UNSPEC so that we can work
	// with either IPv6 or IPv4
	hints.ai_family = AF_UNSPEC;
	// Stream socket (TCP) as opposed to datagram sockets (UDP)
	hints.ai_socktype = SOCK_STREAM;
	// By setting this flag to AI_PASSIVE we can pass NULL for the hostname
	// in getaddrinfo so that the current machine hostname is implied
	//  hints.ai_flags = AI_PASSIVE;

	// This function will return address information for the given:
	// 1. Hostname or IP address (as string in digits-and-dots notation).
	// 2. The port of the address.
	// 3. The struct of hints we supply for the address.
	// 4. The addrinfo struct the function should populate with addresses
	//    (remember that addrinfo is a linked list)
	return_code = getaddrinfo(HOST, READ_PORT, &hints, server_info);

	if (return_code != 0) {
		fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(return_code));
		exit(EXIT_FAILURE);
	}
}

void setup_socket(int socket_descriptor) {
	set_socket_both_buffer_sizes(socket_descriptor);

	// adjust_socket_blocking_timeout(socket_descriptor, 0, 10);
	// if (set_io_flag(socket_descriptor, O_NONBLOCK) == -1) {
	// 	throw("Error setting socket to non-blocking on client-side");
	// }
}

int create_socket() {
	// Address info structs are basic (relatively large) structures
	// containing various pieces of information about a host's address,
	// such as:
	// 1. ai_flags: A set of flags. If we set this to AI_PASSIVE, we can
	//              pass NULL to the later call to getaddrinfo for it to
	//              return the address info of the *local* machine (0.0.0.0)
	// 2. ai_family: The address family, either AF_INET, AF_INET6 or AF_UNSPEC
	//               (the latter makes this struct usable for IPv4 and IPv6)
	//               note that AF stands for Address Family.
	// 3. ai_socktype: The type of socket, either (TCP) SOCK_STREAM with
	//                 connection or (UDP) SOCK_DGRAM for connectionless
	//                 datagrams.
	// 4. ai_protocol: If you want to specify a certain protocol for the socket
	//                 type, i.e. TCP or UDP. By passing 0, the OS will choose
	//                 the most appropriate protocol for the socket type (STREAM
	//                 => TCP, DGRAM = UDP)
	// 5. ai_addrlen: The length of the address. We'll usually not modify this
	//                ourselves, but make use of it after it is populated via
	//                getaddrinfo.
	// 6. ai_addr: The Internet address. This is yet another struct, which
	//             basically contains the IP address and TCP/UDP port.
	// 7. ai_canonname: Canonical hostname.
	// 8. ai_next: This struct is actually a node in a linked list. getaddrinfo
	//             will sometimes return more than one address (e.g. one for IPv4
	//             one for IPv6)
	struct addrinfo *server_info = NULL;

	// The file-descriptor of the socket we will open
	int socket_descriptor;

	get_server_information(&server_info);
	socket_descriptor = get_address(server_info);

	setup_socket(socket_descriptor);

	// Don't need this anymore
	freeaddrinfo(server_info);

	return socket_descriptor;
}

int main(int argc, char *argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty
	CPU_SET(1, &cpuset);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		perror("sched_setaffinity");
		exit(1);
	}
	
	// Sockets are returned by the OS as standard file descriptors.
	// It will be used for all communication with the server.
	int socketDescriptor;

	// Command-line arguments
	struct Arguments args;

	parse_arguments(&args, argc, argv);

	client_once(WAIT);
	
	socketDescriptor = create_socket();
	
	client_once(NOTIFY);
	client_once(WAIT);
	
	communicate(socketDescriptor, &args);

	return EXIT_SUCCESS;
}
