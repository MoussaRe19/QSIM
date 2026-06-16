#ifndef MM1_STATE_H
#define MM1_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "entity.h"
#include "entity_queue.h"
#include "prng.h"
#include "dist.h"
#include "time_acc.h"
#include "sample_acc.h"

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

	double tau_end;

	/* Accumulator activation threshold:
	 * T_warmup = 0 disables warmup delay. */
	bool accumulators_active;
	double T_warmup;

	/* Time-weighted performance metrics */
	TimeAccumulator acc_queue_length; /* Lq: mean queue length */
	TimeAccumulator acc_server_busy;  /* ρ: server utilization */
	TimeAccumulator acc_system_count; /* L: system size */

	/* Per-entity metrics */
	SampleAccumulator acc_waiting_time;  /* Wq: waiting time */
	SampleAccumulator acc_response_time; /* W: total time in system */

	/* Optional callback before entity is freed (NULL in normal use). */
	void (*on_departure)(const Entity *e, double now);

	PRNG prng;
	Distribution arrival_dist;
	Distribution service_dist;
} MM1_State;

#endif