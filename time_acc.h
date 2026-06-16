#ifndef QSIM_TIME_ACC_H
#define QSIM_TIME_ACC_H

/* Time-weighted average of a stepwise value over [tau_start, now).
 *
 * Usage:
 *   tacc_update(now);      // before state change
 *   tacc_set(new_val);     // after state change
 */
typedef struct {
	double tau_start;
	double tau_last;
	double v_current;
	double area;
} TimeAccumulator;

void tacc_init(TimeAccumulator *a, double tau_start, double initial_value);
void tacc_update(TimeAccumulator *a, double now);
void tacc_set(TimeAccumulator *a, double new_value);
double tacc_mean(const TimeAccumulator *a, double now);

#endif
