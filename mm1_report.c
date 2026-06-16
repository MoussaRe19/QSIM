#include <inttypes.h>
#include <stdio.h>
#include "mm1_report.h"
#include "mm1_state.h"

extern MM1_State mm1_state;

MM1_Report mm1_generate_report(void) {
	MM1_Report rep = {0};
	double now = mm1_state.tau_end;

	rep.arrivals_total = mm1_state.arrivals_total;
	rep.completions_total = mm1_state.completions_total;
	rep.max_queue_observed = mm1_state.max_queue_observed;

	if (!mm1_state.accumulators_active) return rep;

	double elapsed = now - mm1_state.acc_queue_length.tau_start;
	if (elapsed <= 0.0) return rep;

	rep.Lq_hat = tacc_mean(&mm1_state.acc_queue_length, now);
	rep.rho_hat = tacc_mean(&mm1_state.acc_server_busy, now);
	rep.L_hat = tacc_mean(&mm1_state.acc_system_count, now);

	if (mm1_state.acc_waiting_time.n > 0) {
		rep.Wq_hat = sacc_mean(&mm1_state.acc_waiting_time);
		rep.Wq_std_err = sacc_std_error(&mm1_state.acc_waiting_time);
	}
	if (mm1_state.acc_response_time.n > 0) {
		rep.W_hat = sacc_mean(&mm1_state.acc_response_time);
		rep.W_std_err = sacc_std_error(&mm1_state.acc_response_time);
	}

	rep.lambda_hat = (double)mm1_state.completions_total / elapsed;
	return rep;
}

void mm1_print_report(const MM1_Report *rep) {
	printf("\n=== M/M/1 Simulation Report ===\n");
	printf("Arrivals            : %" PRIu64 "\n", rep->arrivals_total);
	printf("Completions         : %" PRIu64 "\n", rep->completions_total);
	printf("Max Queue Length    : %d\n", rep->max_queue_observed);

	if (rep->completions_total == 0) {
		printf("No completions — means undefined.\n");
		printf("================================\n");
		return;
	}

	printf("lambda_hat          : %.6f\n", rep->lambda_hat);
	printf("rho_hat             : %.6f\n", rep->rho_hat);
	printf("Lq_hat              : %.6f\n", rep->Lq_hat);
	printf("L_hat               : %.6f\n", rep->L_hat);
	printf("Wq_hat              : %.6f  (SE %.6f)\n", rep->Wq_hat,
	       rep->Wq_std_err);
	printf("W_hat               : %.6f  (SE %.6f)\n", rep->W_hat,
	       rep->W_std_err);
	printf("================================\n");
}
