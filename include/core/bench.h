#pragma once
#include <time.h>
#include <stdio.h>


#define clock_diff(start, stop)                                                                         \
	((stop.tv_nsec < start.tv_nsec)                                                                     \
	? ((stop.tv_sec - start.tv_sec - 1) * 1000000 + (1000000000 + stop.tv_nsec - start.tv_nsec) / 1000) \
	: ((stop.tv_sec - start.tv_sec) * 1000000 + (stop.tv_nsec - start.tv_nsec) / 1000))


#define BENCH_BEGIN()             \
	struct timespec bench_dt_[2]; \
	clock_gettime(CLOCK_MONOTONIC, &bench_dt_[0])

#define BENCH_END()                                              \
    clock_gettime(CLOCK_MONOTONIC, &bench_dt_[1]);               \
    unsigned bench_ds_ = clock_diff(bench_dt_[0], bench_dt_[1]); \
    fprintf(stderr, "benchmark took %u.%03ums\n", bench_ds_ / 1000, bench_ds_ % 1000);
