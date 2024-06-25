#include "fifo/helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "common/common.h"

struct pipeMetaData read_from_pipe(void *buffer, int stream, size_t chunk, size_t numBytesToRead) {
	ssize_t total_bytes_read = 0;
	ssize_t bytes_read = 0;
	
	struct pipeMetaData toReturn = {0, 0, 0};

	if (numBytesToRead == 0) {
		return toReturn;
	}

	if (numBytesToRead < 0) {
		throw("negative bytes to read");
	}

	while (total_bytes_read < numBytesToRead) {
		size_t canRead = chunk;

		if ((numBytesToRead - total_bytes_read) < chunk) {
			canRead = (numBytesToRead - total_bytes_read);
		}

		bytes_read = read(stream, buffer + total_bytes_read, canRead);

		if (bytes_read == 0) {
			break;
		}

		if (bytes_read < 0 && (errno == EAGAIN)) {
			break;
		}

		if (bytes_read < 0)
		{
			exit(1);
		}

		total_bytes_read += bytes_read;
	}

	toReturn.wholeReqs = total_bytes_read / chunk;
	toReturn.spareBytes = total_bytes_read % chunk;
	toReturn.totalBytes = total_bytes_read;

	return toReturn;
}

struct pipeMetaData write_to_pipe(void *buffer, int stream, size_t chunk, size_t numBytesToWrite) {
    ssize_t total_bytes_written = 0;
  	ssize_t bytes_written = 0;
	
	struct pipeMetaData toReturn = {0, 0, 0};


	if (numBytesToWrite == 0) {
		return toReturn;
	}

	if (numBytesToWrite < 0) {
		throw("negative bytes to write");
	}

	while (total_bytes_written < numBytesToWrite) {
		size_t canWrite = chunk;

		if ((numBytesToWrite - total_bytes_written) < chunk) {
			canWrite = (numBytesToWrite - total_bytes_written);
		}

		bytes_written = write(stream, buffer + total_bytes_written, canWrite);

		if (bytes_written == 0) {
			break;
		}		

		if (bytes_written < 0 && (errno == EAGAIN)) {
        	break;
      	}

		if (bytes_written < 0) {
			exit(1);
		}

		total_bytes_written += bytes_written;
	}

	toReturn.wholeReqs = total_bytes_written / chunk;
	toReturn.spareBytes = total_bytes_written % chunk;
	toReturn.totalBytes = total_bytes_written;

	return toReturn;
}

int maxPipeSize(struct Arguments *args) {
	const int MAX_PIPE_SIZE = 104856;
	int messageSize = 8 + args->size;
	int pipeSize = messageSize * args->count;

	if (pipeSize < MAX_PIPE_SIZE) {
		return pipeSize;
	}

	return MAX_PIPE_SIZE;
}