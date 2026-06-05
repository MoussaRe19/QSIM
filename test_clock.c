#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <setjmp.h>
#include "event.h"
#include "kernel.h"
#include "context.h"

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

static jmp_buf fatal_jmp;

static void test_fatal_handler(const char *msg) {
	(void)msg;
	THROW(fatal_jmp, EX_FATAL);
}

#define INSTALL_TEST_FATAL kernel_on_fatal = test_fatal_handler
#define RESTORE_FATAL kernel_on_fatal = kernel_default_fatal

static void dummy_handler(void *ctx, void *data) {
	(void)ctx;
	(void)data;
}

static void test_clock_read_and_advance(void) {
	printf("test_clock_read_and_advance ... ");

	KernelState k;
	kernel_init(&k);

	assert(clock_read(&k) == 0.0);

	clock_advance(&k, 7.5);
	assert(clock_read(&k) == 7.5);

	clock_advance(&k, 20.0);
	assert(clock_read(&k) == 20.0);

	kernel_destroy(&k);

	printf("PASS\n");
}

static void test_context_now(void) {
	printf("test_context_now ... ");

	struct {
		double advance;
		double expected_now;
	} cases[] = {
	    {0.0, 0.0},
	    {12.0, 12.0},
	};

	/* independent test cases (fresh kernel per iteration) */
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		KernelState k;
		Context ctx;

		kernel_init(&k);
		context_init(&ctx, &k);

		if (cases[i].advance > 0.0) {
			clock_advance(&k, cases[i].advance);
		}

		assert(context_now(&ctx) == cases[i].expected_now);
		assert(context_now(&ctx) == clock_read(&k));

		kernel_destroy(&k);
	}

	printf("PASS\n");
}

static void test_schedule_timestamp(void) {
	printf("test_schedule_timestamp ... ");

	struct {
		double initial_clock;
		double delay;
		double expected;
	} cases[] = {
	    {0.0, 5.0, 5.0},
	    {10.0, 5.0, 15.0},
	    {3.0, 0.0, 3.0},
	};

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		KernelState k;
		Context ctx;

		kernel_init(&k);
		context_init(&ctx, &k);

		if (cases[i].initial_clock > 0.0) {
			clock_advance(&k, cases[i].initial_clock);
		}

		int payload = 42;

		EventNotice *e =
		    context_schedule(&ctx, cases[i].delay, dummy_handler, &payload);

		assert(e != NULL);
		assert(e->handler == dummy_handler);
		assert(e->data == &payload);
		assert(e->valid == true);
		assert(e->heap_index >= 0);

		double actual = e->timestamp;
		double expected = clock_read(&k) + cases[i].delay;

		assert(actual == expected);

		free(fel_extract_min(&k.fel));
		kernel_destroy(&k);
	}

	printf("PASS\n");
}

static void test_schedule_event_id_monotonic(void) {
	printf("test_schedule_event_id_monotonic ... ");
	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	EventNotice *e1 = context_schedule(&ctx, 1.0, dummy_handler, NULL);
	EventNotice *e2 = context_schedule(&ctx, 2.0, dummy_handler, NULL);
	EventNotice *e3 = context_schedule(&ctx, 3.0, dummy_handler, NULL);

	assert(e1->id < e2->id && e2->id < e3->id);

	free(fel_extract_min(&k.fel));
	free(fel_extract_min(&k.fel));
	free(fel_extract_min(&k.fel));
	kernel_destroy(&k);
	printf("PASS\n");
}

static void test_clock_advance_same_time_allowed(void) {
	printf("test_clock_advance_same_time_allowed ... ");
	KernelState k;
	kernel_init(&k);

	INSTALL_TEST_FATAL;
	volatile int caught = 0;

	TRY(fatal_jmp) {
		clock_advance(&k, 5.0);
		clock_advance(&k, 5.0);
	}
	CATCH(code) {
		(void)code;
		caught = 1;
	}
	END_TRY;

	RESTORE_FATAL;

	assert(!caught && "clock_advance to the same time must not abort");
	assert(clock_read(&k) == 5.0);

	kernel_destroy(&k);
	printf("PASS\n");
}

static void test_clock_backward_aborts(void) {
	printf("test_clock_backward_aborts (INV1) ... ");
	KernelState k;
	kernel_init(&k);

	INSTALL_TEST_FATAL;
	volatile int caught = 0;

	TRY(fatal_jmp) {
		clock_advance(&k, 5.0);
		clock_advance(&k, 3.0);
	}
	CATCH(code) {
		caught = code;
	}
	END_TRY;

	RESTORE_FATAL;

	assert(caught == EX_FATAL && "Expected fatal for backward clock");
	kernel_destroy(&k);
	printf("PASS\n");
}

static void test_schedule_negative_delay_aborts(void) {
	// INV5: delay < 0 must call the fatal handler
	printf("test_schedule_negative_delay_aborts (INV5) ... ");
	KernelState k;
	Context ctx;
	kernel_init(&k);
	context_init(&ctx, &k);

	INSTALL_TEST_FATAL;
	volatile int caught = 0;

	TRY(fatal_jmp) {
		context_schedule(&ctx, -1.0, dummy_handler, NULL);
	}
	CATCH(code) {
		caught = code;
	}
	END_TRY;

	RESTORE_FATAL;

	assert(caught == EX_FATAL && "Expected fatal for negative delay");
	kernel_destroy(&k);
	printf("PASS\n");
}

static void test_cancel_behavior(void) {
	printf("test_cancel_behavior ... ");

	// Case 1: cancel a later event
	{
		KernelState k;
		Context ctx;
		kernel_init(&k);
		context_init(&ctx, &k);

		EventNotice *e1 = context_schedule(&ctx, 1.0, dummy_handler, NULL);
		EventNotice *e2 = context_schedule(&ctx, 2.0, dummy_handler, NULL);

		context_cancel(e2);
		assert(e2->valid == false);
		e2 = NULL; // ownership transferred to FEL

		EventNotice *extracted = fel_extract_min(&k.fel);
		assert(extracted == e1);
		assert(extracted->valid == true);

		assert(fel_extract_min(&k.fel) == NULL);

		free(extracted); // caller owns extracted event
		kernel_destroy(&k);
	}

	// Case 2: cancel the earliest event
	{
		KernelState k;
		Context ctx;
		kernel_init(&k);
		context_init(&ctx, &k);

		EventNotice *e1 = context_schedule(&ctx, 1.0, dummy_handler, NULL);
		EventNotice *e2 = context_schedule(&ctx, 2.0, dummy_handler, NULL);

		context_cancel(e1);
		assert(e1->valid == false);
		e1 = NULL; // ownership transferred to FEL

		EventNotice *extracted =
		    fel_extract_min(&k.fel); // purges and frees e1 internally
		assert(extracted == e2);
		assert(extracted->valid == true);

		assert(fel_extract_min(&k.fel) == NULL);

		free(extracted);
		kernel_destroy(&k);
	}

	printf("PASS\n");
}

int main(void) {
	printf("\n----- Starting Phase 2 Tests -----\n\n");

	test_clock_read_and_advance();
	test_context_now();
	test_schedule_timestamp();
	test_schedule_event_id_monotonic();
	test_clock_advance_same_time_allowed();
	test_clock_backward_aborts();
	test_schedule_negative_delay_aborts();
	test_cancel_behavior();

	printf("\nAll Phase 2 tests passed.\n");
	return 0;
}