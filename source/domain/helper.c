#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "domain/helper.h"
#include "common/common.h"

struct socketMetaData read_from_socket(void *buffer, int sockfd, size_t chunk, size_t numBytesToRead, bool nonBlock) {
	ssize_t total_bytes_read = 0;
	ssize_t bytes_read = 0;
	
	struct socketMetaData toReturn = {0, 0, 0, 0};

	if (numBytesToRead == 0) {
		return toReturn;
	}

	if (numBytesToRead < 0) {
		return toReturn;
	}

	while (total_bytes_read < numBytesToRead) {
		size_t canRead = chunk;

		if ((numBytesToRead - total_bytes_read) < chunk) {
			canRead = (numBytesToRead - total_bytes_read);
		}

		if (nonBlock) {
			bytes_read = recv(sockfd, buffer + total_bytes_read, canRead, MSG_DONTWAIT);
		} 

		else {
			bytes_read = recv(sockfd, buffer + total_bytes_read, canRead, 0);
		}

		if (bytes_read == 0) {
			break;
		}

		if (bytes_read < 0 && errno == EAGAIN) {
			break;
		}

		if (bytes_read < 0)
		{
			break;
		}

		total_bytes_read += bytes_read;
	}

	toReturn.wholeReqs = total_bytes_read / chunk;
	toReturn.spareBytes = total_bytes_read % chunk;
	toReturn.totalBytes = total_bytes_read;

	return toReturn;
}

struct socketMetaData write_to_socket(void *buffer, int sockfd, size_t chunk, size_t numBytesToWrite, bool nonBlock) {
    ssize_t total_bytes_written = 0;
  	ssize_t bytes_written = 0;
	
	struct socketMetaData toReturn = {0, 0, 0, 0};

	if (numBytesToWrite == 0) {
		return toReturn;
	}

	if (numBytesToWrite < 0) {
		return toReturn;
	}

	while (total_bytes_written < numBytesToWrite) {
		size_t canWrite = chunk;

		if ((numBytesToWrite - total_bytes_written) < chunk) {
			canWrite = (numBytesToWrite - total_bytes_written);
		}

		if (nonBlock) {
			bytes_written = send(sockfd, buffer + total_bytes_written, canWrite, MSG_DONTWAIT);	
		}

		else {
			bytes_written = send(sockfd, buffer + total_bytes_written, canWrite, 0);	
		}
		
		if (bytes_written == 0) {
			break;
		}		

		if (bytes_written < 0 && errno == EAGAIN) {
			break;
      	}

		if (bytes_written < 0) {
			break;
		}

		total_bytes_written += bytes_written;
	}

	toReturn.wholeReqs = total_bytes_written / chunk;
	toReturn.spareBytes = total_bytes_written % chunk;
	toReturn.totalBytes = total_bytes_written;

	return toReturn;
}