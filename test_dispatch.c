#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>

#include "dispatch.h"

// Try Catch:
static jmp_buf dispatch_fatal_jmp;

#define TRY(buf)                                                               \
	do {                                                                       \
		jmp_buf *__env = &(buf);                                               \
		volatile int __ex_val = setjmp(*__env);                                \
		if (__ex_val == 0) {

#define CATCH(err_var)                                                         \
	}                                                                          \
	else {                                                                     \
		int err_var = __ex_val;                                                \
		(void)err_var;

#define END_TRY                                                                \
	}                                                                          \
	}                                                                          \
	while (0)

#define THROW(buf, code) longjmp((buf), (code))
#define EX_FATAL 1

static void test_fatal_handler(const char *msg) {
	(void)msg;
	THROW(dispatch_fatal_jmp, EX_FATAL);
}

#define INSTALL_TEST_FATAL (kernel_on_fatal = test_fatal_handler)
#define RESTORE_FATAL (kernel_on_fatal = kernel_default_fatal)

// Event Handlers Examples for testing
static void noop_handler(void *ctx, void *data) {
	(void)ctx;
	(void)data;
}

static void counter_handler(void *ctx, void *data) {
	(void)ctx;
	int *counter = (int *)data;
	(*counter)++;
}

static void clock_capture_handler(void *ctx, void *data) {
	double *out = (double *)data;
	*out = context_now((Context *)ctx);
}

// Helpers for self-rescheduling tests
typedef struct {
	int count;
	int limit;
	double interval;
} SelfReschedState;

static void self_resched_handler(void *ctx_void, void *data) {
	Context *ctx = (Context *)ctx_void;
	SelfReschedState *s = (SelfReschedState *)data;
	s->count++;
	if (s->count < s->limit)
		context_schedule(ctx, s->interval, self_resched_handler, s);
}

// Helpers for Pridicate-termination tests
static int pred_counter;
static bool pred_stop_at_3(void) {
	return pred_counter >= 3;
}

static void pred_counter_handler(void *ctx, void *data) {
	(void)ctx;
	(void)data;
	pred_counter++;
}

static int rv_pred_counter;
static bool rv_stop_at_2(void) {
	return rv_pred_counter >= 2;
}
static void rv_inc_handler(void *ctx, void *data) {
	(void)ctx;
	(void)data;
	rv_pred_counter++;
}

// TESTS:

static void test_termination_time_based(void) {
	printf("test_termination_time_based ... ");

	KernelState k;
	TerminationCondition tc = tc_time_based(10.0);

	// clock < tau_max  -> false
	kernel_init(&k);
	clock_advance(&k, 5.0);
	assert(termination_met(&k, &tc) == false);
	kernel_destroy(&k);

	// clock == tau_max -> true
	kernel_init(&k);
	clock_advance(&k, 10.0);
	assert(termination_met(&k, &tc) == true);
	kernel_destroy(&k);

	// clock > tau_max -> true
	kernel_init(&k);
	clock_advance(&k, 12.0);
	assert(termination_met(&k, &tc) == true);
	kernel_destroy(&k);

	printf("PASS\n");
}

// Test2: predicate-based
static bool always_false(void) {
	return false;
}
static bool always_true(void) {
	return true;
}

static void test_termination_predicate(void) {
	printf("test_termination_predicate ... ");

	KernelState k;
	kernel_init(&k);

	TerminationCondition tc_f = tc_predicate_based(always_false);
	assert(termination_met(&k, &tc_f) == false);

	TerminationCondition tc_t = tc_predicate_based(always_true);
	assert(termination_met(&k, &tc_t) == true);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test3: empty FEL
static void test_dispatch_empty_fel(void) {
	printf("test_dispatch_empty_fel ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(k.clock == 0.0);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test4: single event
static void test_dispatch_single_event(void) {
	printf("test_dispatch_single_event ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	double captured = -1.0;
	context_schedule(&ctx, 7.5, clock_capture_handler, &captured);

	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(captured == 7.5);
	assert(k.clock == 7.5);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test5:
static void test_dispatch_ordering_and_clock(void) {
	printf("test_dspatch_ordering_and_clock ... ");
	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	double t0 = -1.0, t1 = -1.0, t2 = -1.0;

	context_schedule(&ctx, 9.0, clock_capture_handler, &t2);
	context_schedule(&ctx, 2.0, clock_capture_handler, &t0);
	context_schedule(&ctx, 5.0, clock_capture_handler, &t1);

	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(t0 == 2.0);
	assert(t1 == 5.0);
	assert(t2 == 9.0);
	assert(k.clock == 9.0);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test6 Time-based term:
static void test_dispatch_time_termination(void) {
	printf("test_dispatch_time_termination ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	int fired = 0;
	context_schedule(&ctx, 4.0, counter_handler, &fired);
	context_schedule(&ctx, 8.0, counter_handler, &fired);
	context_schedule(&ctx, 15.0, counter_handler, &fired); /* past horizon */

	TerminationCondition tc = tc_time_based(10.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(fired == 2);
	assert(k.clock == 8.0);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test7: predicate-based term
static void test_dispatch_predicate_termination(void) {
	printf("test_dispatch_predicate_termination ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	pred_counter = 0;
	for (int i = 1; i <= 5; i++)
		context_schedule(&ctx, (double)i, pred_counter_handler, NULL);

	TerminationCondition tc = tc_predicate_based(pred_stop_at_3);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(pred_counter == 3);
	assert(k.clock == 3.0);

	kernel_destroy(&k);
	printf("PASS\n");
}
// TEST8: self-rescheduling each handler schedules its own successor
static void test_dispatch_self_rescheduling(void) {
	printf("test_dispatch_self_rescheduling ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	SelfReschedState s = {.count = 0, .limit = 5, .interval = 2.0};
	context_schedule(&ctx, 0.0, self_resched_handler, &s);

	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(s.count == 5);
	assert(k.clock == 8.0);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test9: cancelled events are silently skipped by the dispatch loop
static void test_dispatch_cancel_skipped(void) {
	printf("test_dispatch_cancel_skipped ... ");

	KernelState k;
	Context ctx;
	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r;

	// Sub-case A: cancel the middle event
	kernel_init(&k);
	context_init(&ctx, &k);
	{
		int fired = 0;
		EventNotice *e1 = context_schedule(&ctx, 1.0, counter_handler, &fired);
		EventNotice *e2 = context_schedule(&ctx, 2.0, counter_handler, &fired);
		EventNotice *e3 = context_schedule(&ctx, 3.0, counter_handler, &fired);
		context_cancel(e2);

		r = dispatch_loop(&ctx, &tc);

		assert(r == INTERPRET_OK);
		assert(fired == 2);
		assert(k.clock == 3.0);
		(void)e1;
		(void)e2;
		(void)e3;
	}
	kernel_destroy(&k);

	/* --- Sub-case B: cancel the first and last events --- */
	kernel_init(&k);
	context_init(&ctx, &k);
	{
		int fired = 0;
		EventNotice *f1 = context_schedule(&ctx, 1.0, counter_handler, &fired);
		EventNotice *f2 = context_schedule(&ctx, 2.0, counter_handler, &fired);
		EventNotice *f3 = context_schedule(&ctx, 3.0, counter_handler, &fired);
		context_cancel(f1);
		context_cancel(f3);

		r = dispatch_loop(&ctx, &tc);

		assert(r == INTERPRET_OK);
		assert(fired == 1);
		assert(k.clock == 2.0);
		(void)f1;
		(void)f2;
		(void)f3;
	}
	kernel_destroy(&k);

	printf("PASS\n");
}

// Test10:
static void test_dispatch_all_cancelled(void) {
	printf("test_dispatch_all_cancelled ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	int fired = 0;
	EventNotice *e1 = context_schedule(&ctx, 1.0, counter_handler, &fired);
	EventNotice *e2 = context_schedule(&ctx, 2.0, counter_handler, &fired);
	EventNotice *e3 = context_schedule(&ctx, 3.0, counter_handler, &fired);
	context_cancel(e1);
	context_cancel(e2);
	context_cancel(e3);

	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(fired == 0);
	assert(k.clock == 0.0); // clock must not have moved
	(void)e1;
	(void)e2;
	(void)e3;

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test11 :
static void test_dispatch_zero_delay_event(void) {
	printf("test_dispatch_zero_delay_event ... ");

	KernelState k;
	Context ctx;
	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r;

	/* --- Sub-case A: single event at delay=0 --- */
	kernel_init(&k);
	context_init(&ctx, &k);
	{
		double captured = -1.0;
		context_schedule(&ctx, 0.0, clock_capture_handler, &captured);

		r = dispatch_loop(&ctx, &tc);

		assert(r == INTERPRET_OK);
		assert(captured == 0.0);
		assert(k.clock == 0.0);
	}
	kernel_destroy(&k);

	/* --- Sub-case B: zero-interval self-rescheduling chain --- */
	kernel_init(&k);
	context_init(&ctx, &k);
	{
		SelfReschedState s = {.count = 0, .limit = 4, .interval = 0.0};
		context_schedule(&ctx, 0.0, self_resched_handler, &s);

		r = dispatch_loop(&ctx, &tc);

		assert(r == INTERPRET_OK);
		assert(s.count == 4);
		assert(k.clock == 0.0); /* all events at t=0; clock must not drift */
	}
	kernel_destroy(&k);

	printf("PASS\n");
}

// Test12:
static void test_dispatch_simultaneous_timestamps(void) {
	printf("test_dispatch_simultaneous_timestamps ... ");

	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	int fired = 0;
	context_schedule(&ctx, 5.0, counter_handler, &fired);
	context_schedule(&ctx, 5.0, counter_handler, &fired);
	context_schedule(&ctx, 5.0, counter_handler, &fired);

	TerminationCondition tc = tc_time_based(100.0);
	InterpretResult r = dispatch_loop(&ctx, &tc);

	assert(r == INTERPRET_OK);
	assert(fired == 3);
	assert(k.clock == 5.0);

	kernel_destroy(&k);
	printf("PASS\n");
}

// Test13: return value
static void test_dispatch_return_values(void) {
	printf("test_dispatch_return_values ... ");

	KernelState k;
	Context ctx;
	TerminationCondition tc;
	InterpretResult r;

	/* (a) natural FEL drain */
	kernel_init(&k);
	context_init(&ctx, &k);
	tc = tc_time_based(100.0);
	context_schedule(&ctx, 1.0, noop_handler, NULL);
	r = dispatch_loop(&ctx, &tc);
	assert(r == INTERPRET_OK);
	kernel_destroy(&k);

	/* (b) time-based horizon */
	kernel_init(&k);
	context_init(&ctx, &k);
	tc = tc_time_based(10.0);
	{
		int fired = 0;
		context_schedule(&ctx, 5.0, counter_handler, &fired);
		context_schedule(&ctx, 50.0, counter_handler, &fired);
		r = dispatch_loop(&ctx, &tc);
		assert(r == INTERPRET_OK);
		assert(fired == 1);
	}
	kernel_destroy(&k);

	/* (c) predicate termination */
	kernel_init(&k);
	context_init(&ctx, &k);
	tc = tc_predicate_based(rv_stop_at_2);
	rv_pred_counter = 0;
	for (int i = 1; i <= 5; i++)
		context_schedule(&ctx, (double)i, rv_inc_handler, NULL);
	r = dispatch_loop(&ctx, &tc);
	assert(r == INTERPRET_OK);
	assert(k.clock == 2);
	kernel_destroy(&k);

	printf("PASS\n");
}

// TEST14: (INV2)
/*
 * Bypasses the public API to inject an invalid past event.
 * Cannot occur in normal usage; used to verify fatal detection.
 */
static void test_dispatch_causality_abort(void) {
	printf("test_dispatch_causality_abort (INV2) ... ");

	KernelState k;
	Context ctx; 
	kernel_init(&k);
	context_init(&ctx, &k);

	clock_advance(&k, 10.0);

	// bypass context_schedule.
	EventNotice *past = (EventNotice *)malloc(sizeof(EventNotice));
	past->id = k.event_id_ctr++;
	past->timestamp = 5.0;
	past->handler = noop_handler;
	past->data = NULL;
	past->valid = true;
	past->heap_index = -1;
	fel_insert(&k.fel, past);

	INSTALL_TEST_FATAL;
	volatile int caught = 0;

	TRY(dispatch_fatal_jmp) {
		TerminationCondition tc = tc_time_based(100.0);
		dispatch_loop(&ctx, &tc);
	}
	CATCH(code) {
		caught = code;
	}
	END_TRY;

	RESTORE_FATAL;
	assert(caught == EX_FATAL);


	// Manual cleanup: the loop aborted before freeing the event.
	free(past);
	k.fel.size = 0; // prevent kernel_destroy from double-freeing
	kernel_destroy(&k);

	printf("PASS\n");
}


static void test_dispatch_event_exactly_at_tau_max(void) {
    printf("test_dispatch_event_exactly_at_tau_max ... ");

    KernelState k;
    Context ctx;
    kernel_init(&k);
    context_init(&ctx, &k);

    int fired = 0;
    context_schedule(&ctx, 10.0, counter_handler, &fired);
    context_schedule(&ctx, 10.1, counter_handler, &fired);

    TerminationCondition tc = tc_time_based(10.0);
    InterpretResult r = dispatch_loop(&ctx, &tc);

    assert(r == INTERPRET_OK);
    assert(fired == 1);
    assert(k.clock == 10.0);

    kernel_destroy(&k);
    printf("PASS\n");
}

static void test_dispatch_cancelled_root_beyond_horizon(void) {
    printf("test_dispatch_cancelled_root_beyond_horizon ... ");

    KernelState k;
    Context ctx;
    kernel_init(&k);
    context_init(&ctx, &k);

    int fired = 0;
    context_schedule(&ctx, 5.0, counter_handler, &fired);  // valid, within horizon 
    EventNotice *e = context_schedule(&ctx, 50.0, counter_handler, &fired); // beyond
    context_cancel(e);
    e = NULL;

    TerminationCondition tc = tc_time_based(10.0);
    InterpretResult r = dispatch_loop(&ctx, &tc);

    assert(r == INTERPRET_OK);
    assert(fired == 1);      // only the valid in-horizon event fires
    assert(k.clock == 5.0);  // clock must not jump to canceled event

    kernel_destroy(&k);
    printf("PASS\n");
}

int main(void) {
	printf("\n----- Phase 3 Test Suite -----\n\n");

	test_termination_time_based();
	test_termination_predicate();
	test_dispatch_empty_fel();
	test_dispatch_single_event();
	test_dispatch_ordering_and_clock();
	test_dispatch_time_termination();
	test_dispatch_predicate_termination();
	test_dispatch_self_rescheduling();
	test_dispatch_cancel_skipped();
	test_dispatch_all_cancelled();
	test_dispatch_zero_delay_event();
	test_dispatch_simultaneous_timestamps();
	test_dispatch_return_values();
	test_dispatch_causality_abort();
	test_dispatch_event_exactly_at_tau_max();
	test_dispatch_cancelled_root_beyond_horizon();


	printf("\nAll Phase 3 tests passed.\n");
	return 0;
}