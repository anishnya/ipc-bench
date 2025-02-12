#include "common/parent.h"
#include "tcp/helper.h"
#include <signal.h>

int makeFifo(const char* path) {
	remove(path);
	int fd;
	fd = mkfifo(path, 0666);
	if (fd > 0) {
		return -1;
	}

	return fd;
}

int main(int argc, char* argv[]) {
	makeFifo(TCP_FIFO_PATH);
	setup_parent("tcp", argc, argv);
}
