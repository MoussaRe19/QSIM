#ifndef QSIM_CONTEXT_H
#define QSIM_CONTEXT_H

#include "kernel.h"

// Termination condition
typedef bool (*predicate_fn_t)(void);

typedef enum { TERM_TIME_BASED, TERM_PREDICATE_BASED } TerminationType;

typedef struct {
	KernelState *kernel;
} Context;

// Lifecycle
void context_init(Context *ctx, KernelState *k);

// model-facing API
double context_now(const Context *ctx);
EventNotice *context_schedule(Context *ctx, double delay,
                              event_handler_t handler, void *data);

void context_cancel(EventNotice *e);

#endif