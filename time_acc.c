#include "time_acc.h"

void tacc_init(TimeAccumulator *a, double tau_start, double initial_value) {
	a->tau_start = tau_start;
	a->tau_last = tau_start;
	a->v_current = initial_value;
	a->area = 0.0;
}

// Weighted
void tacc_update(TimeAccumulator *a, double now) {
	a->area += a->v_current * (now - a->tau_last);
	a->tau_last = now;
}

void tacc_set(TimeAccumulator *a, double new_value) {
	a->v_current = new_value;
}

/* Closes the current open segment [tau_last, now) without mutating. */
double tacc_mean(const TimeAccumulator *a, double now) {
	double elapsed = now - a->tau_start;
	if (elapsed <= 0.0) return 0.0;
	return (a->area + a->v_current * (now - a->tau_last)) / elapsed;
}
