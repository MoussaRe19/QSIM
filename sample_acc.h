#ifndef SAMPLE_ACC_H
#define SAMPLE_ACC_H

#include <stdint.h>

/* Online mean and variance (Welford's algorithm). */
typedef struct {
	uint64_t n;
	double mean;
	double M2; // sum of squared deviations
} SampleAccumulator;

void sacc_init(SampleAccumulator *a);
void sacc_add(SampleAccumulator *a, double x);
double sacc_mean(const SampleAccumulator *a);
double sacc_variance(const SampleAccumulator *a);
double sacc_std_error(const SampleAccumulator *a); // SE = sqrt(var/n)

#endif
