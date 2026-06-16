#include <stdlib.h>
#include <stdio.h>

#include "context.h"

void context_init(Context *ctx, KernelState *k) {
	ctx->kernel = k;
}

double context_now(const Context *ctx) {
	return ctx->kernel->clock;
}

EventNotice *context_schedule(Context *ctx, double delay,
                              event_handler_t handler, void *data) {

	if (ctx == NULL || ctx->kernel == NULL) {
		kernel_on_fatal("NULL context passed to context_schedule");
		return NULL;
	}

	if (delay < 0.0) {
		kernel_on_fatal("INV5: negative delay passed to context_schedule");
		return NULL;
	}

	double scheduled_time = ctx->kernel->clock + delay;

	EventNotice *e = (EventNotice *)malloc(sizeof(EventNotice));
	if (!e) {
		kernel_on_fatal("OUT OF MEMORY in context_schedule");
		return NULL;
	}

	e->id = ctx->kernel->event_id_ctr++;
	e->timestamp = scheduled_time;
	e->handler = handler;
	e->data = data;
	e->valid = true;
	e->heap_index = -1;

	fel_insert(&ctx->kernel->fel, e);
	return e;
}

void context_cancel(EventNotice *e) {
	fel_cancel(e);
}