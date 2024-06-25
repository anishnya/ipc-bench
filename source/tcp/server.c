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

void print_address(struct addrinfo *address_info) {
	char *type;
	void *address;
	char ip[INET6_ADDRSTRLEN];

	// IPv4
	if (address_info->ai_family == AF_INET) {
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)address_info->ai_addr;
		address = &(ipv4->sin_addr);
		type = "IPv4";
	} else {// IPv6
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)address_info->ai_addr;
		address = &(ipv6->sin6_addr);
		type = "IPv6";
	}

	inet_ntop(address_info->ai_family, address, ip, sizeof ip);
	printf("%s: %s ... ", type, ip);
}

void handle_blocking(int socket_descriptor) {
	// Will be necessary when calling setsockopt to free busy sockets
	int yes = 1;

	// Reclaim blocked but unused sockets (from zombie processes)
	// clang-format off
	int return_code = setsockopt(
		socket_descriptor,
		SOL_SOCKET,
		SO_REUSEADDR,
		&yes,
		sizeof yes
	 );
	// clang-format on

	if (return_code == -1) {
		throw("Error reclaiming socket");
	}
}

struct addrinfo *
get_address(struct addrinfo *server_info, int *socket_descriptor) {
	struct addrinfo *valid_address;
	int return_code;

	// Iterate through the address linked-list until
	// we find one we can get a socket for
	for (valid_address = server_info; valid_address != NULL;
			 valid_address = valid_address->ai_next) {
		// The call to socket() is the first step in establishing a socket
		// based communication. It takes the following arguments:
		// 1. The address family (PF_INET or PF_INET6)
		// 2. The socket type (SOCK_STREAM or SOCK_DGRAM)
		// 3. The protocol (TCP or UDP)
		// Note that all of these fields will have been already populated
		// by getaddrinfo. If the call succeeds, it returns a valid file descriptor.
		// clang-format off
		*socket_descriptor = socket(
			valid_address->ai_family,
			valid_address->ai_socktype,
			valid_address->ai_protocol
		 );
		// clang-format on

		if (*socket_descriptor == -1) {
			continue;
		}

		handle_blocking(*socket_descriptor);

		// Once we have a socket, we need to bind it to an address.
		// Again, this information we get from the addrinfo struct
		// that was populated by getaddrinfo(). The arguments are:
		// 1. The socket file_descriptor to which to bind the address.
		// 2. The address to bind (sockaddr_in struct)
		// 3. The size of this address structure.
		// clang-format off
		return_code = bind(
			*socket_descriptor,
			valid_address->ai_addr,
			valid_address->ai_addrlen
		);
		// clang-format on

		if (return_code == 1) {
			close(*socket_descriptor);
		}

		break;
	}

	// Return the valid address info
	return valid_address;
}

void cleanup(int descriptor) {
	close(descriptor);
}

void setup_socket(int socket_descriptor) {
	set_socket_both_buffer_sizes(socket_descriptor);
}

int accept_communication(int socket_descriptor) {
	// Data type big enough to hold both an sockaddr_in and sockaddr_in6 structure
	// The ai_addr structure contained in the addrinfo struct can point to either
	// an IPv4 sockaddr_in or an IPv6 sockaddr_in6 struct. Sometimes, we don't
	// know which will be returned from or which we have to pass to a function, so
	// the sockaddr_storage struct is large enough to hold either (you can then
	// cast as necessary)
	struct sockaddr_storage other_address;

	// Data type to store the size of an sockaddr_storage object
	socklen_t sin_size = sizeof other_address;

	// Start accepting connections on this address and machine.
	// This call will block until a client connects to the
	// socket_descriptor, and then returns a new file descriptor
	// just for communicating with this specific client. All
	// communication will then go through that socket. The server
	// could then just fork off a child to handle the client
	// and start accepting new clients on the socket_descriptor
	// clang-format off

	int connection = accept(
		socket_descriptor,
		(struct sockaddr *)&other_address,
		&sin_size
	);
	// clang-format on

	if (connection == -1) {
		throw("Error accepting");
	}

	
	setup_socket(connection);

	// Don't need the main server descriptor anymore at this
	// point because we'll only communicate to the one client
	// for this benchmark.
	close(socket_descriptor);


	return connection;
}

void communicate(int descriptor, struct Arguments *args) {
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

		readInfo = read_from_socket(readBuffer, descriptor, reqSize, toRead, false);
		totalBytesRead += readInfo.totalBytes;
		outstandingBytes += readInfo.totalBytes;

		if (readInfo.totalBytes != 0) {
			toWrite += readInfo.totalBytes;
		}

		writeInfo = write_to_socket(readBuffer, descriptor, reqSize, toWrite, false);
		totalBytesWritten += writeInfo.totalBytes;
		outstandingBytes -= writeInfo.totalBytes;
	}
	
	// before closing connection, wait for client to finish
	server_once(WAIT);
	evaluateServer(&bench, args->count);
	close(descriptor);
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


int create_socket() {
	// Sockets are returned by the OS as standard file descriptors.
	// The first socket will be for the server's main connection port.
	// For every client that connects to that port (at that socket), we
	// get a new file descriptor just for communicating with precisely
	// that client.
	int socket_descriptor;

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
	struct addrinfo *valid_address;

	get_server_information(&server_info);
	valid_address = get_address(server_info, &socket_descriptor);

	// Don't need this anymore
	freeaddrinfo(server_info);

	// If we didn't actually find a valid address
	if (valid_address == NULL) {
		throw("Error finding valid address");
	}

	// Now that we have a socket and an address bound to it, we
	// need to tell the OS that we wish to start listening for
	// incoming connections on this port.
	// Allow up to ten connections to queue up until the server
	// accepts them (the OS limit is often between 10 and 20)

	if (listen(socket_descriptor, 10) == 1) {
		throw("Error listening on given socket!");
	}

	return socket_descriptor;
}

int main(int argc, char *argv[]) {
	cpu_set_t cpuset;  // Set of CPUs
  	CPU_ZERO(&cpuset);  // Initialize the set to empty

	// Set the CPU affinity to core 0 (first core)
	CPU_SET(0, &cpuset);

	if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
		perror("sched_setaffinity");
		exit(1);
	}
	
	// File-descriptor to the server socket
	int socket_descriptor;

	// File-descriptor for the socket over which
	// client-communication will take place
	int connection;

	// Command line arguments
	struct Arguments args;

	parse_arguments(&args, argc, argv);

	socket_descriptor = create_socket();
	
	server_once(NOTIFY);
	server_once(WAIT);
	
	connection = accept_communication(socket_descriptor);
	
	server_once(NOTIFY);

	communicate(connection, &args);

	return EXIT_SUCCESS;
}
