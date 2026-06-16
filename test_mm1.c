#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "dist.h"
#include "entity.h"
#include "mm1_state.h"
#include "mm1_init.h"
#include "mm1_report.h"

/* Statistical tolerance parameters for convergence checks. */
#define AUTOCORR_FACTOR                                                        \
	4.0 /* Inflate IID standard error for autocorrelation. */
#define TIME_AVG_REL_TOL 0.05 /* Relative tolerance for rho, Lq, and L. */
#define LITTLES_REL_TOL 0.02  /* Relative tolerance for Little's Law checks. */
#define SE_MAX_FRACTION 0.10  /* Maximum acceptable SE relative to the mean. */

extern MM1_State mm1_state;

// Test1:
static void test_mm1_basic_run(void) {
	printf("test_mm1_basic_run ... ");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 42};
	mm1_init(cfg);

	InterpretResult r = mm1_run(10000.0);
	assert(r == INTERPRET_OK);

	MM1_Report rep = mm1_generate_report();
	mm1_print_report(&rep);

	double lambda = 1.0 / cfg.arrival_mean;
	double mu = 1.0 / cfg.service_mean;
	double rho = lambda / mu;

	assert(rep.completions_total > 0);
	assert(rep.arrivals_total >= rep.completions_total);
	assert(rep.max_queue_observed >= 0);

	assert(rep.W_hat >= rep.Wq_hat);
	assert(rep.L_hat >= rep.Lq_hat);

	assert(rep.rho_hat > 0.0 && rep.rho_hat <= 1.0);

	assert(rep.lambda_hat > 0.0);

	/* Little's Law: L ≈ λ·W and Lq ≈ λ·Wq.
	 * Use L_hat as the absolute scale for both tolerances.
	 */
	assert(fabs(rep.L_hat - rep.lambda_hat * rep.W_hat) <=
	       LITTLES_REL_TOL * rep.L_hat);
	assert(fabs(rep.Lq_hat - rep.lambda_hat * rep.Wq_hat) <=
	       LITTLES_REL_TOL * rep.L_hat);

	(void)rho;

	printf("PASS\n");
}

// Test2:
static void test_mm1_theoretical_values(void) {
	printf("test_mm1_theoretical_values ... ");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 42};
	double run_length = 100000.0;

	mm1_init(cfg);
	mm1_run(run_length);

	double lambda = 1.0 / cfg.arrival_mean;
	double mu = 1.0 / cfg.service_mean;
	double rho = lambda / mu;

	/* ensure stability */
	assert(rho < 1.0);

	/* ── theoretical means ── */
	double expected_rho = rho;
	double expected_wq = rho / (mu * (1.0 - rho));
	double expected_w = expected_wq + 1.0 / mu;
	double expected_lq = rho * rho / (1.0 - rho);
	double expected_l = rho / (1.0 - rho);

	/* ── theoretical per‑customer variances ── */
	double var_wq = rho * (2.0 - rho) / (mu * mu * (1.0 - rho) * (1.0 - rho));
	double var_w = var_wq + 1.0 / (mu * mu);

	MM1_Report rep = mm1_generate_report();
	mm1_print_report(&rep);

	assert(rep.completions_total > 0);
	double n = (double)rep.completions_total;

	/* Inflate the naive IID standard error to account for autocorrelation. */
	double tol_wq = AUTOCORR_FACTOR * sqrt(var_wq / n);
	double tol_w = AUTOCORR_FACTOR * sqrt(var_w / n);

	assert(rep.Wq_hat >= expected_wq - tol_wq &&
	       rep.Wq_hat <= expected_wq + tol_wq);
	assert(rep.W_hat >= expected_w - tol_w && rep.W_hat <= expected_w + tol_w);

	/* Compare against the theoretical mean rather than the estimate. */
	assert(rep.Wq_std_err > 0.0 &&
	       rep.Wq_std_err < SE_MAX_FRACTION * expected_wq);
	assert(rep.W_std_err > 0.0 && rep.W_std_err < SE_MAX_FRACTION * expected_w);

	assert(fabs(rep.rho_hat - expected_rho) <= TIME_AVG_REL_TOL * expected_rho);
	assert(fabs(rep.Lq_hat - expected_lq) <= TIME_AVG_REL_TOL * expected_lq);
	assert(fabs(rep.L_hat - expected_l) <= TIME_AVG_REL_TOL * expected_l);

	/* Little's Law consistency checks. */
	assert(fabs(rep.L_hat - rep.lambda_hat * rep.W_hat) <=
	       LITTLES_REL_TOL * rep.L_hat);
	assert(fabs(rep.Lq_hat - rep.lambda_hat * rep.Wq_hat) <=
	       LITTLES_REL_TOL * rep.L_hat);

	printf("PASS (n=%.0f, Wq=%.4f [±%.4f], W=%.4f [±%.4f], "
	       "rho=%.4f, Lq=%.4f, L=%.4f)\n",
	       n, rep.Wq_hat, tol_wq, rep.W_hat, tol_w, rep.rho_hat, rep.Lq_hat,
	       rep.L_hat);
}

// Test3:
#define MAX_TRACE 64
static double trace_departures[MAX_TRACE];
static uint64_t trace_ids[MAX_TRACE];
static int trace_count = 0;

static void record_departure(const Entity *e, double now) {
	if (trace_count < MAX_TRACE) {
		trace_departures[trace_count] = now;
		trace_ids[trace_count] = e->id;
		trace_count++;
	}
}

static void test_mm1_deterministic_trace(void) {
	printf("test_mm1_deterministic_trace ... ");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 0};
	double tau_max = 10.0;

	mm1_init(cfg);
	mm1_state.arrival_dist = dist_deterministic(cfg.arrival_mean);
	mm1_state.service_dist = dist_deterministic(cfg.service_mean);

	trace_count = 0;
	mm1_state.on_departure = record_departure;

	InterpretResult r = mm1_run(tau_max);
	assert(r == INTERPRET_OK);

	int n_completions_expected = 0;
	double expected_dep[MAX_TRACE];

	for (int i = 0; i < MAX_TRACE; i++) {
		double dep = (double)i * cfg.arrival_mean + cfg.service_mean;
		if (dep > tau_max) break;

		expected_dep[i] = dep;
		n_completions_expected++;
	}

	int n_arrivals_expected = 0;
	for (int i = 0; i < MAX_TRACE; i++) {
		double arr = (double)i * cfg.arrival_mean;
		if (arr > tau_max) break;

		n_arrivals_expected++;
	}

	double rho_det = cfg.service_mean / cfg.arrival_mean;
	double lambda_det = (double)n_completions_expected / tau_max;

	assert(trace_count == n_completions_expected);
	assert(mm1_state.completions_total == (uint64_t)n_completions_expected);

	for (int i = 0; i < n_completions_expected; i++)
		assert(trace_departures[i] == expected_dep[i]);

	for (int i = 1; i < trace_count; i++)
		assert(trace_ids[i] > trace_ids[i - 1]);

	assert(mm1_state.max_queue_observed == 0);

	MM1_Report rep = mm1_generate_report();
	mm1_print_report(&rep);

	assert(rep.completions_total == (uint64_t)n_completions_expected);
	assert(rep.arrivals_total == (uint64_t)n_arrivals_expected);
	assert(rep.max_queue_observed == 0);

	/* Deterministic service with no queueing yields zero variance. */
	assert(rep.Wq_hat == 0.0);
	assert(rep.Wq_std_err == 0.0);
	assert(rep.W_hat == cfg.service_mean);
	assert(rep.W_std_err == 0.0);

	/* Time-averaged quantities are exact for this schedule. */
	assert(rep.rho_hat == rho_det);
	assert(rep.Lq_hat == 0.0);
	assert(rep.L_hat == rho_det);
	assert(rep.lambda_hat == lambda_det);

	assert(fabs(rep.L_hat - rep.lambda_hat * rep.W_hat) < 1e-9);

	printf("PASS\n");
}

// Test4:
static void test_mm1_queueing_occurs(void) {
	printf("test_mm1_queueing_occurs ... ");

	MM1_Config cfg = {.arrival_mean = 0.5, .service_mean = 1.0, .seed = 7};
	double T = 50.0;

	double lambda = 1.0 / cfg.arrival_mean;
	double mu = 1.0 / cfg.service_mean;

	assert(lambda > mu);

	mm1_init(cfg);
	InterpretResult r = mm1_run(T);
	assert(r == INTERPRET_OK);

	MM1_Report rep = mm1_generate_report();
	mm1_print_report(&rep);

	double exp_arrivals = lambda * T;
	double exp_capacity = mu * T;
	double exp_excess = (lambda - mu) * T;

	assert((double)rep.arrivals_total >=
	       exp_arrivals - 4.0 * sqrt(exp_arrivals));

	assert((double)rep.completions_total <=
	       exp_capacity + 4.0 * sqrt(exp_capacity));

	assert(rep.arrivals_total >= rep.completions_total);

	uint64_t backlog = rep.arrivals_total - rep.completions_total;
	assert((double)backlog >= 0.5 * exp_excess);
	assert((double)rep.max_queue_observed >= 0.5 * exp_excess);

	assert(rep.Wq_hat > 0.0);

	double min_rho = 0.90;
	assert(rep.rho_hat >= min_rho);

	/* Renewal-process lower bound on utilization. */
	double renewal_lb = 1.0 - 4.0 / sqrt(mu * T);
	assert(rep.rho_hat >= renewal_lb);

	assert(rep.Lq_hat >= 0.5 * exp_excess);

	printf("PASS\n");
}

/* ── Test 5: zero completions guard ──────────────────────────────────── */
/*
 * tau_max so small that no departure fires.
 * completions_total must be 0
 */
static void test_mm1_zero_completions(void) {
	printf("test_mm1_zero_completions ... ");

	MM1_Config cfg = {.arrival_mean = 1.0, .service_mean = 100.0, .seed = 1};
	mm1_init(cfg);

	double tau_max = cfg.service_mean * 1e-4;
	InterpretResult r = mm1_run(tau_max);

	assert(r == INTERPRET_OK);
	assert(mm1_state.completions_total == 0);
	assert(mm1_state.arrivals_total >= 1);
	assert(mm1_state.in_service == NULL); /* Teardown released the entity. */

	MM1_Report rep = mm1_generate_report();
	mm1_print_report(&rep);

	assert(rep.completions_total == 0);
	assert(rep.arrivals_total >= 1);

	assert(rep.Wq_hat == 0.0);
	assert(rep.W_hat == 0.0);
	assert(rep.Wq_std_err == 0.0);
	assert(rep.W_std_err == 0.0);
	assert(rep.rho_hat == 0.0);
	assert(rep.Lq_hat == 0.0);
	assert(rep.L_hat == 0.0);

	printf("PASS\n");
}

// Test6
#define N_REPS 30
#define TAU_MAX 10000.0
#define Z_95 1.96 /* 95% confidence interval z-score */

static void test_mm1_replication_ci(void) {
	printf("test_mm1_replication_ci\n\n");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0};

	double lambda = 1.0 / cfg.arrival_mean;
	double mu = 1.0 / cfg.service_mean;
	double rho = lambda / mu;
	double analytical_wq = rho / (mu * (1.0 - rho));

	assert(rho < 1.0);

	SampleAccumulator acc_wq;
	sacc_init(&acc_wq);

	for (int i = 0; i < N_REPS; i++) {
		cfg.seed = (uint64_t)i;
		mm1_init(cfg);
		mm1_run(TAU_MAX);

		MM1_Report rep = mm1_generate_report();
		assert(rep.completions_total > 0);

		sacc_add(&acc_wq, rep.Wq_hat);
		printf("  rep %2d  seed=%2d  Wq_hat=%.4f\n", i, i, rep.Wq_hat);
	}

	double mean_wq = sacc_mean(&acc_wq);
	double se_wq = sacc_std_error(&acc_wq);
	double ci_lo = mean_wq - Z_95 * se_wq;
	double ci_hi = mean_wq + Z_95 * se_wq;

	printf("\n  reps    = %d\n", N_REPS);
	printf("  mean_Wq = %.6f\n", mean_wq);
	printf("  SE      = %.6f\n", se_wq);
	printf("  95%% CI  = [%.6f, %.6f]\n", ci_lo, ci_hi);
	printf("  theory  = %.6f\n\n", analytical_wq);

	assert(ci_lo <= analytical_wq && analytical_wq <= ci_hi);

	printf("PASS\n");
}

int main(void) {
	printf("\n----- Phase 5 Test Suite -----\n\n");

	test_mm1_basic_run();
	test_mm1_theoretical_values();
	test_mm1_deterministic_trace();
	test_mm1_queueing_occurs();
	test_mm1_zero_completions();

	printf("\nAll Phase 5 tests passed.\n");

	/* Informational summary — not asserted */
	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 42};
	mm1_init(cfg);
	mm1_run(10000.0);

	MM1_Report rep = mm1_generate_report();
	mm1_print_report(&rep);

	test_mm1_replication_ci();

	return 0;
}