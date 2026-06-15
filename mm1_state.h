#ifndef MM1_STATE_H
#define MM1_STATE_H

#include <stdint.h>
#include "entity.h"
#include "entity_queue.h"
#include "prng.h"
#include "dist.h"

typedef enum { SERVER_IDLE, SERVER_BUSY } ServerStatus;

typedef struct {
	ServerStatus server_status;
	int queue_length;
	EntityQueue waiting_queue;

	/* Entity being served (NULL if idle).
	 * Needed for cleanup when the run ends before its departure event. */
	Entity *in_service;

	uint64_t arrivals_total;
	uint64_t completions_total;
	int max_queue_observed;

	/* Temporary running sums; Phase 6 will introduce proper accumulators. */
	double wait_time_sum;
	double response_time_sum; // total time an entity spends in the system

	/* Optional callback before entity is freed (NULL in normal use). */
	void (*on_departure)(const Entity *e, double now);

	PRNG prng;
	Distribution arrival_dist;
	Distribution service_dist;
} MM1_State;

#endif