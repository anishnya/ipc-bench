#include "common/shmSetup.h"

char* create_mem(int size) {
    key_t segment_key;
    int segment_id;

	segment_key = generate_key("shm");

    segment_id = shmget(segment_key, size, IPC_CREAT | 0666);

	if (segment_id < 0) {
		throw("Could not get segment");
	}

    return (char*)shmat(segment_id, NULL, 0);
}

void shm_cleanup(int segment_id, char* shared_memory) {
	shmdt(shared_memory);
	shmctl(segment_id, IPC_RMID, NULL);
}