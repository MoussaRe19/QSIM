#ifndef QSIM_MM1_REPORT_H
#define QSIM_MM1_REPORT_H

#include <stdint.h>

typedef struct {
	uint64_t arrivals_total;
	uint64_t completions_total;
	int max_queue_observed;

	double lambda_hat; /* throughput: completions / active elapsed  */
	double rho_hat;    /* time-average server utilization            */
	double Lq_hat;     /* time-average queue length (waiting only)   */
	double L_hat;      /* time-average system count (Little's L)     */
	double Wq_hat;     /* sample-mean wait in queue                  */
	double W_hat;      /* sample-mean sojourn time                   */
	double Wq_std_err;
	double W_std_err;
} MM1_Report;

MM1_Report mm1_generate_report(void);
void mm1_print_report(const MM1_Report *rep);

#endif
