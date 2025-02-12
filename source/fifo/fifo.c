#include "common/parent.h"
#include "fifo/helper.h"
#include <signal.h>
#include <stdio.h>

int makeFifo(const char* path) {
	remove(path);
	int fd;
	fd = mkfifo(path, 0666);
	if (fd > 0) {
		printf("errno %d\n", errno);
		return -1;
	}

	return fd;
}

int main(int argc, char* argv[]) {
	// Setup FIFOs
	makeFifo(REQUEST);
	makeFifo(RESPONSE);
	setup_parent("fifo", argc, argv);
}
