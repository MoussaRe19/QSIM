#include <assert.h>
#include <stdlib.h>
#include "context.h"
#include "dist.h"
#include "mm1_state.h"
#include "mm1_handlers.h"

/* File-static closure: handlers access model state directly.
 * This is owned and managed by mm1_init.c */
extern MM1_State mm1_state;

/* One-time initialization of time accumulators from current state. */
static void latch_accumulators(double now) {
	if (mm1_state.accumulators_active || now < mm1_state.T_warmup) return;

	int busy = (mm1_state.server_status == SERVER_BUSY) ? 1 : 0;
	tacc_init(&mm1_state.acc_queue_length, now, (double)mm1_state.queue_length);
	tacc_init(&mm1_state.acc_server_busy, now, (double)busy);
	tacc_init(&mm1_state.acc_system_count, now,
	          (double)(mm1_state.queue_length + busy));
	mm1_state.accumulators_active = true;
}

static void tacc_update_all(double now) {
	tacc_update(&mm1_state.acc_queue_length, now);
	tacc_update(&mm1_state.acc_server_busy, now);
	tacc_update(&mm1_state.acc_system_count, now);
}

static void tacc_set_all(void) {
	int busy = (mm1_state.server_status == SERVER_BUSY) ? 1 : 0;
	tacc_set(&mm1_state.acc_queue_length, (double)mm1_state.queue_length);
	tacc_set(&mm1_state.acc_server_busy, (double)busy);
	tacc_set(&mm1_state.acc_system_count,
	         (double)(mm1_state.queue_length + busy));
}

void handler_arrival(void *context, void *data) {
	(void)data;
	Context *ctx = (Context *)context;
	double now = context_now(ctx);

	latch_accumulators(now);
	if (mm1_state.accumulators_active) tacc_update_all(now);

	Entity *e = entity_create(now);
	mm1_state.arrivals_total++;

	if (mm1_state.server_status == SERVER_IDLE) {
		mm1_state.server_status = SERVER_BUSY;
		e->service_start_time = now;
		mm1_state.in_service = e;

		double s = dist_sample(&mm1_state.service_dist, &mm1_state.prng);
		context_schedule(ctx, s, handler_departure, e);
	} else {
		mm1_state.queue_length++;
		if (mm1_state.queue_length > mm1_state.max_queue_observed)
			mm1_state.max_queue_observed = mm1_state.queue_length;

		entity_queue_push(&mm1_state.waiting_queue, e);
	}

	if (mm1_state.accumulators_active) tacc_set_all();

	double a = dist_sample(&mm1_state.arrival_dist, &mm1_state.prng);
	context_schedule(ctx, a, handler_arrival, NULL);
}

void handler_departure(void *context, void *data) {
	Context *ctx = (Context *)context;
	double now = context_now(ctx);
	Entity *e = (Entity *)data;

	latch_accumulators(now);
	if (mm1_state.accumulators_active) tacc_update_all(now);

	double wait = e->service_start_time - e->arrival_time;
	double response_time = now - e->arrival_time;

	if (mm1_state.accumulators_active) {
		sacc_add(&mm1_state.acc_waiting_time, wait);
		sacc_add(&mm1_state.acc_response_time, response_time);
	}

	if (mm1_state.on_departure) mm1_state.on_departure(e, now);

	mm1_state.completions_total++;
	mm1_state.in_service = NULL;

	entity_destroy(e);

	assert(mm1_state.queue_length >= 0 && "INV6: queue_length went negative");

	if (mm1_state.queue_length > 0) {
		mm1_state.queue_length--;

		Entity *next = entity_queue_pop(&mm1_state.waiting_queue);
		next->service_start_time = now;
		mm1_state.in_service = next;

		double s = dist_sample(&mm1_state.service_dist, &mm1_state.prng);
		context_schedule(ctx, s, handler_departure, next);
	} else {
		mm1_state.server_status = SERVER_IDLE;
	}

	if (mm1_state.accumulators_active) tacc_set_all();
}