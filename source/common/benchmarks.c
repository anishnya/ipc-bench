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

// Function used by qsort to compare two unsigned long longs
int compareULLongs(const void *a, const void *b) {
  return (*(unsigned long long*)a - *(unsigned long long*)b);
}

void analyze_array(unsigned long long arr[], size_t size, size_t rate) {
  double sum = 0.0;
  int i;

  // Find the sum of all elements, prevent overflow
  for (i = 0; i < size; i++) {
	double toAdd = (arr[i] / (double)size);
    sum += toAdd;
  }

  // Calculate the mean (average)
  double mean = sum;

  // Find the minimum and maximum elements
  unsigned long long min = arr[0];
  unsigned long long max = arr[0];
  for (i = 1; i < size; i++) {
    if (arr[i] < min) {
      min = arr[i];
    } else if (arr[i] > max) {
      max = arr[i];
    }
  }

  // Sort the array to find the median
  qsort(arr, size, sizeof(unsigned long long), compareULLongs);

  // Calculate the median
  double median;
  if (size % 2 == 0) {
    median = (arr[size / 2 - 1] + arr[size / 2]) / 2.0;
  } else {
    median = arr[size / 2];
  }

  // Calculate the standard deviation
  double sq_diff = 0.0;
  for (i = 0; i < size; i++) {
    sq_diff += pow(arr[i] - mean, 2);
  }
  double std = sqrt(sq_diff / size);

  // Print the results
  printf("Latency = %.2f\n", mean);
  printf("Median = %.2f\n", median);
  printf("Standard Deviation = %.2f\n", std);
  printf("Minimum = %llu\n", min); // Use %llu format specifier for unsigned long long
  printf("Maximum = %llu\n", max);
  printf("expected total time: %f\n", ((mean / rate) * size));
}


void evaluate(Benchmarks* bench, Arguments* args) {
	assert(args->count > 0);
	const bench_t total_time = now() - bench->total_start;
	const double average = (((double)total_time) / args->count);

	printf("Total duration:     %.3f\tms\n", total_time / 1e6);
	printf("Latency:   %.3f\tus\n", average / 1000.0);
}

void evaluateClient(bench_t *diffs, Arguments* args) {
	assert(args->count > 0);
	analyze_array((unsigned long long*)diffs, args->count, args->rate);
}

void evaluateServer(Benchmarks *bench, size_t numReqs) {
	const bench_t total_time = now() - bench->total_start;
	int messageRate = (int)(numReqs / (total_time / 1e9));
	printf("Total duration:     %.3f\tms\n", total_time / 1e6);
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
