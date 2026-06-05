#include <stdlib.h>
#include <stdio.h>

#include "dispatch.h"

#define DISPATCH_EXTRACT_NEXT(kernel) fel_extract_min(&(kernel)->fel)

#define DISPATCH_ASSERT_CAUSALITY(kernel, e)                                   \
	do {                                                                       \
		if ((e)->timestamp < (kernel)->clock) {                                \
			kernel_on_fatal("INV2: causal consistency violated — "             \
			                "extracted event timestamp < current clock");      \
			return INTERPRET_RUNTIME_ERROR;                                    \
		}                                                                      \
	} while (0)

#define DISPATCH_ADVANCE_CLOCK(kernel, e)                                      \
	clock_advance((kernel), (e)->timestamp)

#define DISPATCH_FIRE(ctx, e) (e)->handler((ctx), (e)->data)

#define FREE_EVENT(e) free((e))

#ifdef QSIM_TRACE_DISPATCH
#define DISPATCH_TRACE(kernel, e)                                              \
	fprintf(stderr, "[TRACE] t=%.6f  id=%-4llu  handler=%p\n",                 \
	        (kernel)->clock, (unsigned long long)(e)->id,                      \
	        (void *)(e)->handler)

#else
#define DISPATCH_TRACE(kernel, e) ((void)0)
#endif

#define RUNTIME_ERROR(msg)                                                     \
	do {                                                                       \
		kernel_on_fatal(msg);                                                  \
		return INTERPRET_RUNTIME_ERROR;                                        \
	} while (0)

bool termination_met(const KernelState *kernel,
                     const TerminationCondition *tc) {
	switch (tc->type) {
	case TERM_TIME_BASED:
		return kernel->clock >= tc->value.tau_max;

	case TERM_PREDICATE_BASED:
		if (tc->value.predicate_fn == NULL) {
			kernel_on_fatal("INV: TERM_PREDICATE_BASED with NULL predicate");
			return true;
		}
		return tc->value.predicate_fn();
	}

	kernel_on_fatal("termination_met: unknown TerminationType");
	return true;
}

InterpretResult dispatch_loop(Context *ctx, const TerminationCondition *tc) {
	for (;;) {

		// Horizon guard
		if (tc->type == TERM_TIME_BASED &&
		    fel_peek_timestamp_dispatch(&ctx->kernel->fel, tc->value.tau_max) >
		        tc->value.tau_max)
			break;

		// DESIGN RATIONALE:
		// Termination predicates must observe post-dispatch state.
		// Revisit if termination semantics change.
		//
		// Historical pre-check (removed):
		// if (termination_met(ctx->kernel, tc)) break;

		// Extract next valid event; NULL means FEL is drained.
		EventNotice *e = DISPATCH_EXTRACT_NEXT(ctx->kernel);
		if (e == NULL) break;

		// Ensure event timestamp is monotonic (INV2).
		DISPATCH_ASSERT_CAUSALITY(ctx->kernel, e);

		// Advance simulation clock before running the handler.
		DISPATCH_ADVANCE_CLOCK(ctx->kernel, e);

		DISPATCH_TRACE(ctx->kernel, e);

		// Execute opaque handler logic.
		DISPATCH_FIRE(ctx, e);

		/* Event lifetime ends here; copy any needed data during DISPATCH_FIRE.
		 */
		FREE_EVENT(e);

		// TODO: Defensive post-check. Currently redundant with the loop
		// pre-check, but preserves an immediate termination point if future
		// trailing logic (logging, cleanup, next-event extraction, etc.) is
		// added below.
		if (termination_met(ctx->kernel, tc)) break;
	}

	return INTERPRET_OK;
}

#undef DISPATCH_EXTRACT_NEXT
#undef DISPATCH_ASSERT_CAUSALITY
#undef DISPATCH_ADVANCE_CLOCK
#undef DISPATCH_FIRE
#undef FREE_EVENT
#undef DISPATCH_TRACE
#undef RUNTIME_ERROR
