#include <assert.h>
#include <stdlib.h>
#include "context.h"
#include "dist.h"
#include "mm1_state.h"
#include "mm1_handlers.h"

/* File-static closure: handlers access model state directly.
 * This is owned and managed by mm1_init.c */
extern MM1_State mm1_state;

void handler_arrival(void *context, void *data) {
	(void)data;
	Context *ctx = (Context *)context;
	double now = context_now(ctx);

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

	double a = dist_sample(&mm1_state.arrival_dist, &mm1_state.prng);
	context_schedule(ctx, a, handler_arrival, NULL);
}

void handler_departure(void *context, void *data) {
	Context *ctx = (Context *)context;
	double now = context_now(ctx);
	Entity *e = (Entity *)data;

	double wait = e->service_start_time - e->arrival_time;
	double response_time = now - e->arrival_time;

	mm1_state.wait_time_sum += wait;
	mm1_state.response_time_sum += response_time;

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
}