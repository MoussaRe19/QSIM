#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "dist.h"
#include "entity.h"
#include "mm1_state.h"
#include "mm1_init.h"

extern MM1_State mm1_state;

// Test1:
static void test_mm1_basic_run(void) {
	printf("test_mm1_basic_run ... ");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 42};
	mm1_init(cfg);

	InterpretResult r = mm1_run(10000.0);

	assert(r == INTERPRET_OK);
	assert(mm1_state.completions_total > 0);
	assert(mm1_state.arrivals_total >= mm1_state.completions_total);
	assert(mm1_state.queue_length >= 0);
	assert((uint64_t)mm1_state.queue_length <=
	       (uint64_t)mm1_state.max_queue_observed);
	assert(mm1_state.wait_time_sum >= 0.0);
	assert(mm1_state.response_time_sum >= mm1_state.wait_time_sum);

	printf("PASS\n");
}

/*
 * Test 2: Dynamic M/M/1 validation.
 * Computes theoretical waiting/sojourn means and variances from λ, μ, and ρ,
 * then compares simulation results using a tolerance of ±4√(Var/n),
 * where the factor 4 compensates for queue autocorrelation.
 */
static void test_mm1_theoretical_values(void) {
	printf("test_mm1_theoretical_values ... ");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 42};
	double run_length = 10000.0;

	mm1_init(cfg);
	mm1_run(run_length);

	/* ── derive rates and load ── */
	double lambda = 1.0 / cfg.arrival_mean;
	double mu = 1.0 / cfg.service_mean;
	double rho = lambda / mu;

	/* ensure stability */
	assert(rho < 1.0);

	/* ── theoretical means ── */
	double expected_wq = rho / (mu * (1.0 - rho));
	double expected_w = expected_wq + 1.0 / mu;

	/* ── theoretical per‑customer variances ── */
	double var_wq = rho * (2.0 - rho) / (mu * mu * (1.0 - rho) * (1.0 - rho));
	double var_w = var_wq + 1.0 / (mu * mu);

	/* ── observed sample size ── */
	double n = (double)mm1_state.completions_total;
	assert(n > 0); /* must have at least one completion */

	/* Conservative tolerance: 4× the naive IID standard error.
	 * This is a heuristic to avoid false failures from queue autocorrelation;
	 * it is not a statistically derived standard error. */
	double tol_wq = 4.0 * sqrt(var_wq / n);
	double tol_w = 4.0 * sqrt(var_w / n);

	/* ── simulated means ── */
	double mean_wait = mm1_state.wait_time_sum / n;
	double mean_resp = mm1_state.response_time_sum / n;

	assert(mean_wait >= expected_wq - tol_wq &&
	       mean_wait <= expected_wq + tol_wq);
	assert(mean_resp >= expected_w - tol_w && mean_resp <= expected_w + tol_w);

	printf("PASS (n=%.0f, Wq=%.4f [±%.4f], W=%.4f [±%.4f])\n", n, mean_wait,
	       tol_wq, mean_resp, tol_w);
}

// Test3:
#define MAX_TRACE 8
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

/*
 * Deterministic(2.0) arrivals, Deterministic(1.0) service, rho=0.5.
 * Arrivals occur at 0,2,4,6,8 and depart at 1,3,5,7,9.
 * No queueing or waiting should occur.
 */
static void test_mm1_deterministic_trace(void) {
	printf("test_mm1_deterministic_trace ... ");

	MM1_Config cfg = {.arrival_mean = 2.0, .service_mean = 1.0, .seed = 0};
	mm1_init(cfg);
	mm1_state.arrival_dist = dist_deterministic(2.0);
	mm1_state.service_dist = dist_deterministic(1.0);

	trace_count = 0;
	mm1_state.on_departure = record_departure;

	InterpretResult r = mm1_run(10.0);

	assert(r == INTERPRET_OK);

	/* Exactly five departures before t=10 */
	assert(trace_count == 5);
	assert(mm1_state.completions_total == 5);

	static const double expected_departures[] = {1.0, 3.0, 5.0, 7.0, 9.0};

	for (int i = 0; i < 5; i++)
		assert(trace_departures[i] == expected_departures[i]);

	/* FIFO: departure order must match arrival order */
	for (int i = 1; i < trace_count; i++)
		assert(trace_ids[i] > trace_ids[i - 1]);

	/* Server is always idle when an arrival occurs */
	assert(mm1_state.max_queue_observed == 0);
	assert(mm1_state.wait_time_sum == 0.0);

	printf("PASS\n");
}
/*
 * Test 4: Heavy-load (ρ = 2) validation.
 * Verifies that arrivals exceed service capacity, causing sustained queue
 * growth. Checks arrivals, completions, backlog, and maximum queue size
 * against statistically derived bounds based on Poisson/Renewal behavior.
 */
static void test_mm1_queueing_occurs(void) {
	printf("test_mm1_queueing_occurs ... ");

	const double arrival_mean = 0.5;
	const double service_mean = 1.0;
	const double T = 50.0;

	const double lambda = 1.0 / arrival_mean; /* 2.0 */
	const double mu = 1.0 / service_mean;     /* 1.0 */

	MM1_Config cfg = {
	    .arrival_mean = arrival_mean, .service_mean = service_mean, .seed = 7};
	mm1_init(cfg);

	InterpretResult r = mm1_run(T);
	assert(r == INTERPRET_OK);

	const double exp_arrivals = lambda * T;      /* 100 */
	const double exp_capacity = mu * T;          /* 50  */
	const double exp_excess = (lambda - mu) * T; /* 50  */

	/* Arrivals: Poisson(λT) — 4σ lower bound = λT − 4√(λT) = 60 */
	assert((double)mm1_state.arrivals_total >=
	       exp_arrivals - 4.0 * sqrt(exp_arrivals));

	/* Completions: Renewal(μT) upper bound — 4σ = μT + 4√(μT) ≈ 78 */
	assert((double)mm1_state.completions_total <=
	       exp_capacity + 4.0 * sqrt(exp_capacity));

	/* Conservation: departures can never exceed arrivals */
	assert(mm1_state.arrivals_total >= mm1_state.completions_total);

	/* Under overload (λ > μ), backlog grows at rate λ−μ.
	 * Require at least half the expected drift: B ≥ ½(λ−μ)T
	 * (roughly a 2σ safety margin for these parameters). */
	uint64_t backlog = mm1_state.arrivals_total - mm1_state.completions_total;
	assert((double)backlog >= 0.5 * exp_excess);

	/* max_queue_observed ≥ queue at T ≥ backlog − 1 (entity in service),
	 * so the same half-drift lower bound holds */
	assert((double)mm1_state.max_queue_observed >= 0.5 * exp_excess);

	/* Positive waiting is mandatory under overload */
	assert(mm1_state.wait_time_sum > 0.0);

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

	InterpretResult r = mm1_run(0.0001);

	assert(r == INTERPRET_OK);
	assert(mm1_state.completions_total == 0);

	/* at least one entity arrived but never finished —
	 * server must be busy and in_service must have been cleaned up by mm1_run
	 */
	assert(mm1_state.arrivals_total >= 1);
	assert(mm1_state.in_service == NULL); /* mm1_run teardown freed it */

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
	mm1_print_counts();

	return 0;
}