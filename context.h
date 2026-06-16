#ifndef QSIM_CONTEXT_H
#define QSIM_CONTEXT_H

#include "kernel.h"

// Termination condition
typedef bool (*predicate_fn_t)(void);

// Future:
// TERM_MAX_EVENTS      — stop after k dispatched events (replay/debug)
// TERM_HYBRID          — TIME_BASED AND PREDICATE_BASED (whichever first)
// TERM_EXTERNAL_SIGNAL — volatile flag, useful for interactive UIs

typedef enum { TERM_TIME_BASED, TERM_PREDICATE_BASED } TerminationType;

typedef struct {
	TerminationType type;
	union {
		double tau_max;
		predicate_fn_t predicate_fn;
	} value;
} TerminationCondition;

typedef struct {
	KernelState *kernel;
} Context;

// Constructors
static inline TerminationCondition tc_time_based(double tau_max) {
	TerminationCondition tc;
	tc.type = TERM_TIME_BASED;
	tc.value.tau_max = tau_max;
	return tc;
}

static inline TerminationCondition tc_predicate_based(predicate_fn_t fn) {
	TerminationCondition tc;
	tc.type = TERM_PREDICATE_BASED;
	tc.value.predicate_fn = fn;
	return tc;
}

// Lifecycle
void context_init(Context *ctx, KernelState *k);

// model-facing API
double context_now(const Context *ctx);
EventNotice *context_schedule(Context *ctx, double delay,
                              event_handler_t handler, void *data);

void context_cancel(EventNotice *e);

#endif