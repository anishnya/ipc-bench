#include "common/rateGuard.h"

void rate_wait(atomic_char* guard) {
	while (atomic_load(guard) == 'f')
		;
}

void rate_full_notify(atomic_char* guard) {
	atomic_store(guard, 'f');
}

void rate_empty_notify(atomic_char* guard) {
	atomic_store(guard, 'e');
}

bench_t readTimeFromBuff(void *buffer) {
    bench_t time;
    memcpy(&time, buffer, sizeof(bench_t));
    return time;
}

void writeTimeToBuff(void *buffer) {
    bench_t time = now();
    memcpy(buffer, &time, sizeof(bench_t));
}