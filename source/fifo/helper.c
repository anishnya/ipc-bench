#include "fifo/helper.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "common/common.h"

void read_from_pipe(void *buffer, int stream, size_t num_bytes) {
	ssize_t total_bytes_read = 0;
	ssize_t bytes_read;

	while (total_bytes_read < num_bytes) {
		bytes_read = read(stream, buffer + total_bytes_read, num_bytes - total_bytes_read);

		if (bytes_read < 0 && errno == EAGAIN) {
			continue;
		}

		if (bytes_read < 0)
		{
			continue;
		}

		total_bytes_read += bytes_read;
	}
}

void write_to_pipe(void *buffer, int stream, size_t num_bytes) {
    ssize_t total_bytes_written = 0;
  	ssize_t bytes_written;

	while (total_bytes_written < num_bytes) {
		bytes_written = write(stream, buffer + total_bytes_written, num_bytes - total_bytes_written);	
		
		if (bytes_written < 0 && errno == EAGAIN) {
        	continue;
      	}

		if (bytes_written < 0) {
			throw("error");
		}		

		total_bytes_written += bytes_written;
	}

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