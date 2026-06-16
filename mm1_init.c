#include <inttypes.h>
#include <stdio.h>
#include "mm1_state.h"
#include "mm1_handlers.h"
#include "mm1_init.h"
#include "context.h"
#include "dispatch.h"
#include "prng.h"
#include "dist.h"

MM1_State mm1_state;

void mm1_init(MM1_Config cfg) {
	prng_init(&mm1_state.prng, cfg.seed);

	mm1_state.server_status = SERVER_IDLE;
	mm1_state.queue_length = 0;
	entity_queue_init(&mm1_state.waiting_queue);
	mm1_state.in_service = NULL;

	mm1_state.arrivals_total = 0;
	mm1_state.completions_total = 0;
	mm1_state.max_queue_observed = 0;

	mm1_state.T_warmup = 0.0;
	mm1_state.tau_end = 0.0;
	mm1_state.accumulators_active = false;
	sacc_init(&mm1_state.acc_waiting_time);
	sacc_init(&mm1_state.acc_response_time);

	mm1_state.on_departure = NULL;

	mm1_state.arrival_dist = dist_exponential(cfg.arrival_mean);
	mm1_state.service_dist = dist_exponential(cfg.service_mean);
}

InterpretResult mm1_run(double tau_max) {
	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	context_schedule(&ctx, 0.0, handler_arrival, NULL);

	TerminationCondition tc = tc_time_based(tau_max);
	InterpretResult r = dispatch_loop(&ctx, &tc);
	mm1_state.tau_end = context_now(&ctx);

	/* Simulation ended with entities still alive — free them. */
	if (mm1_state.in_service != NULL) {
		entity_destroy(mm1_state.in_service);
		mm1_state.in_service = NULL;
	}

	entity_queue_destroy(&mm1_state.waiting_queue);

	kernel_destroy(&k);
	return r;
}
