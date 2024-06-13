#ifndef IPC_BENCH_BENCHMARKS_H
#define IPC_BENCH_BENCHMARKS_H

#include <stdio.h>
#include <stdlib.h>
struct Arguments;

typedef unsigned long long bench_t;

typedef struct Benchmarks {
	// Start of the total benchmarking
	bench_t total_start;

	// Start of single benchmark
	bench_t single_start;

	// Minimum time
	bench_t minimum;

	// Maximum time
	bench_t maximum;

	// Sum (for averaging)
	bench_t sum;

	// Squared sum (for standard deviation)
	bench_t squared_sum;

} Benchmarks;

bench_t now();

void setup_benchmarks(Benchmarks *bench);

void benchmark(Benchmarks *bench);

void evaluate(Benchmarks *bench, struct Arguments *args);

void evaluateServer(Benchmarks *bench, size_t numReqs);

void benchmarkCon(Benchmarks* bench, bench_t startTime);

void calculate_average_and_percentile(bench_t *data, size_t size);

void evaluateClient(bench_t *diffs, struct Arguments* args);

#endif /* IPC_BENCH_BENCHMARKS_H */
