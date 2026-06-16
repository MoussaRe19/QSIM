#include <math.h>
#include "sample_acc.h"

void sacc_init(SampleAccumulator *a) {
	a->n = 0;
	a->mean = 0.0;
	a->M2 = 0.0;
}

void sacc_add(SampleAccumulator *a, double x) {
	a->n++;
	double delta = x - a->mean;
	a->mean += delta / (double)a->n;
	a->M2 += delta * (x - a->mean); /* Welford: delta × delta2 */
}

double sacc_mean(const SampleAccumulator *a) {
	return (a->n > 0) ? a->mean : 0.0;
}

double sacc_variance(const SampleAccumulator *a) {
	return (a->n >= 2) ? a->M2 / (double)(a->n - 1) : 0.0;
}

double sacc_std_error(const SampleAccumulator *a) {
	return (a->n >= 1) ? sqrt(sacc_variance(a) / (double)a->n) : 0.0;
}
