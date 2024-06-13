#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "common/arguments.h"
#include "common/benchmarks.h"

bench_t now() {
#ifdef __MACH__
	return ((double)clock()) / CLOCKS_PER_SEC * 1e9;
#else
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);

	return ts.tv_sec * 1e9 + ts.tv_nsec;

#endif
}

void setup_benchmarks(Benchmarks* bench) {
	bench->minimum = INT32_MAX;
	bench->maximum = 0;
	bench->sum = 0;
	bench->squared_sum = 0;
	bench->total_start = now();
}

void benchmark(Benchmarks* bench) {
	const bench_t time = now() - bench->single_start;

	if (time < bench->minimum) {
		bench->minimum = time;
	}

	if (time > bench->maximum) {
		bench->maximum = time;
	}

	bench->sum += time;
	bench->squared_sum += (time * time);
}

void benchmarkCon(Benchmarks* bench, bench_t startTime) {
	const bench_t time = now() - startTime;
	if (time < bench->minimum) {
		bench->minimum = time;
	}

	if (time > bench->maximum) {
		bench->maximum = time;
	}

	bench->sum += time;
	bench->squared_sum += (time * time);
}

void evaluate(Benchmarks* bench, Arguments* args) {
	assert(args->count > 0);
	const double average = (((double)bench->sum) / args->count);

	printf("Latency:   %.3f\tus\n", average / 1000.0);
}

void evaluateClient(bench_t *diffs, Arguments* args) {
	assert(args->count > 0);
	double sum = 0;
	for (size_t i = 0; i < args->count; ++i) {
		sum += (double)diffs[i];
	}

	const double average = sum / args->count;
	printf("Latency:   %.3f\tus\n", average / 1000.0);
}

void evaluateServer(Benchmarks *bench, size_t numReqs) {
	const bench_t total_time = now() - bench->total_start;
	int messageRate = (int)(numReqs / (total_time / 1e9));
	printf("Message rate:       %d\tmsg/s\n", messageRate);
}

// Optional comparison function for qsort (ascending order)
int compar(const void *a, const void *b) {
  return (*(bench_t*)a - *(bench_t*)b);
}


void calculate_average_and_percentile(bench_t *data, size_t size) {
	// Calculate the average
	double sum = 0.0;
	for (size_t i = 0; i < size; i++) {
		sum += data[i];
	}
  	
	double average = sum / size;
	printf("Latency:   %.3f\tus\n", average / 1000.0);
	return;
  	
	// Sort the array to find the percentile value
 	qsort(data, size, sizeof(bench_t), (int (*)(const void*, const void*))compar);

 	 // Calculate the index for the 99th percentile
  	size_t percentile_index = (int)((99.0 / 100.0) * size);

  	// Handle edge cases for small arrays
	if (percentile_index == size) {
		percentile_index = size - 1; // Use the last element for 100th percentile
	} else if (percentile_index == 0) {
		percentile_index = 0; // Use the first element for 0th percentile
	}

  	// Return the 99th percentile value
	printf("P99 Latency:   %.3f\tus\n", (double)data[percentile_index] / 1000.0);
  	return;
}
