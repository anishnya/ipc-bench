#include "common/parent.h"
#include "domain/helper.h"
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
	makeFifo(SOCKET_FIFO_PATH);
	setup_parent("domain", argc, argv);
}
