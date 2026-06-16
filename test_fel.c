#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>
#include "fel.h"

static uint64_t next_id = 1;

static EventNotice *make_event(double timestamp) {
	EventNotice *e = (EventNotice *)malloc(sizeof(EventNotice));
	assert(e != NULL);
	e->id = next_id++;
	e->timestamp = timestamp;
	e->handler = NULL;
	e->data = NULL;
	e->valid = true;
	e->heap_index = -1;
	return e;
}

static void free_event(EventNotice *e) {
	free(e);
}

static void test_basic_order(void) {
	printf("test_basic_order ... ");
	FEL fel;
	fel_init(&fel);

	double timestamps[] = {5.0, 1.0, 8.0, 3.0, 2.0};
	EventNotice *events[5];

	for (int i = 0; i < 5; i++) {
		events[i] = make_event(timestamps[i]);
		fel_insert(&fel, events[i]);
	}

	double expected[] = {1.0, 2.0, 3.0, 5.0, 8.0};
	for (int i = 0; i < 5; i++) {
		EventNotice *e = fel_extract_min(&fel);
		assert(e != NULL);
		assert(e->timestamp == expected[i]);
		free_event(e);
	}
	assert(fel_extract_min(&fel) == NULL);

	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_cancel_skipped(void) {
	printf("test_cancel_skipped ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(1.0);
	EventNotice *e2 = make_event(2.0);
	EventNotice *e3 = make_event(3.0);

	fel_insert(&fel, e1);
	fel_insert(&fel, e2);
	fel_insert(&fel, e3);

	fel_cancel(e2);
	e2 = NULL; // ownership transferred, poison the pointer

	EventNotice *r1 = fel_extract_min(&fel);
	assert(r1 != NULL && r1->timestamp == 1.0);

	EventNotice *r2 = fel_extract_min(&fel);
	assert(r2 != NULL && r2->timestamp == 3.0);

	assert(fel_extract_min(&fel) == NULL);

	free_event(e1);
	free_event(e3);

	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_tie_break_by_id(void) {
	printf("test_tie_break_by_id ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(5.0);
	EventNotice *e2 = make_event(5.0);

	fel_insert(&fel, e1);
	fel_insert(&fel, e2);

	EventNotice *first = fel_extract_min(&fel);
	assert(first != NULL);

	EventNotice *second = fel_extract_min(&fel);
	assert(second != NULL);

	free_event(e1);
	free_event(e2);

	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_peek_empty(void) {
	printf("test_peek_empty ... ");
	FEL fel;
	fel_init(&fel);

	assert(isinf(fel_peek(&fel)));

	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_peek_valid(void) {
	printf("test_peek_valid ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(4.0);
	EventNotice *e2 = make_event(2.0);
	fel_insert(&fel, e1);
	fel_insert(&fel, e2);

	assert(fel_peek(&fel) == 2.0);
	assert(fel_peek(&fel) == 2.0);
	assert(fel.size == 2);

	free_event(fel_extract_min(&fel));
	free_event(fel_extract_min(&fel));
	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_peek_after_cancel(void) {
	printf("test_peek_after_cancel ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(1.0);
	EventNotice *e2 = make_event(2.0);
	fel_insert(&fel, e1);
	fel_insert(&fel, e2);

	fel_cancel(e1);
	e1 = NULL;

	assert(fel_peek(&fel) == 2.0);

	free_event(fel_extract_min(&fel));
	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_cancel_all(void) {
	printf("test_cacel_all ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(1.0);
	EventNotice *e2 = make_event(2.0);
	fel_insert(&fel, e1);
	fel_insert(&fel, e2);

	fel_cancel(e1);
	e1 = NULL;
	fel_cancel(e2);
	e2 = NULL;

	assert(isinf(fel_peek(&fel)));
	assert(fel_extract_min(&fel) == NULL);

	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_heap_index_after_insert(void) {
	printf("test_heap_index_after_insert ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(5.0);
	EventNotice *e2 = make_event(1.0);
	EventNotice *e3 = make_event(3.0);
	fel_insert(&fel, e1);
	fel_insert(&fel, e2);
	fel_insert(&fel, e3);

	for (int i = 0; i < fel.size; i++) {
		assert(fel.heap[i]->heap_index == i);
	}

	fel_destroy(&fel); // destroys FEL and frees all stored events
	printf("PASS\n");
}

static void test_heap_index_after_extract(void) {
	printf("test_heap_index_after_extract ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(5.0);
	EventNotice *e2 = make_event(1.0);
	EventNotice *e3 = make_event(3.0);

	fel_insert(&fel, e1);
	fel_insert(&fel, e2);
	fel_insert(&fel, e3);

	EventNotice *extracted = fel_extract_min(&fel);
	assert(extracted->heap_index == -1);

	for (int i = 0; i < fel.size; i++)
		assert(fel.heap[i]->heap_index == i);

	free_event(extracted);

	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_heap_index_after_cancel_purge(void) {
	printf("test_heap_index_after_cancel_purge ... ");
	FEL fel;
	fel_init(&fel);

	EventNotice *e1 = make_event(1.0);
	EventNotice *e2 = make_event(2.0);
	fel_insert(&fel, e1);
	fel_insert(&fel, e2);

	fel_cancel(e1);
	e1 = NULL;

	// peek triggers lazy deletion: e1 is purged and freed internally
	double next = fel_peek(&fel);
	assert(next == 2.0);
	assert(fel.size == 1);
	assert(fel.heap[0]->heap_index == 0);

	free_event(fel_extract_min(&fel));
	fel_destroy(&fel);
	printf("PASS\n");
}

static void test_reschedule_earlier_and_later(void) {
	printf("test_reschedule_earlier_and_later ... ");
	FEL fel;

	// Scenario 1: Reschedule Earlier (Move last-inserted event to new minimum)
	fel_init(&fel);

	EventNotice *e1 = make_event(5.0);
	EventNotice *e2 = make_event(8.0);
	EventNotice *e3 = make_event(12.0);

	fel_insert(&fel, e1);
	fel_insert(&fel, e2);
	fel_insert(&fel, e3);

	/* Move e3 from 12.0 -> 1.0 (should become the new minimum). */
	fel_reschedule(&fel, e3, 1.0);

	/* Verify heap_index consistency. */
	for (int i = 0; i < fel.size; i++) {
		assert(fel.heap[i]->heap_index == i);
	}

	EventNotice *first = fel_extract_min(&fel);
	assert(first == e3 && first->timestamp == 1.0);

	EventNotice *second = fel_extract_min(&fel);
	assert(second == e1 && second->timestamp == 5.0);

	EventNotice *third = fel_extract_min(&fel);
	assert(third == e2 && third->timestamp == 8.0);

	free_event(e1);
	free_event(e2);
	free_event(e3);
	fel_destroy(&fel);

	// Scenario 2: Reschedule Later (Move minimum event to a later time)
	fel_init(&fel);

	e1 = make_event(2.0);
	e2 = make_event(5.0);
	e3 = make_event(9.0);

	fel_insert(&fel, e1);
	fel_insert(&fel, e2);
	fel_insert(&fel, e3);

	/* Move e1 from 2.0 -> 7.0 (should sink between e2 and e3). */
	fel_reschedule(&fel, e1, 7.0);

	for (int i = 0; i < fel.size; i++) {
		assert(fel.heap[i]->heap_index == i);
	}

	first = fel_extract_min(&fel);
	assert(first->timestamp == 5.0);

	second = fel_extract_min(&fel);
	assert(second->timestamp == 7.0);

	third = fel_extract_min(&fel);
	assert(third->timestamp == 9.0);

	free_event(e1);
	free_event(e2);
	free_event(e3);
	fel_destroy(&fel);

	printf("PASS\n");
}

static void test_reschedule_heap_index_consistency(void) {
	printf("test_reschedule_heap_index_consistency ... ");
	FEL fel;
	fel_init(&fel);

#define NUM_EVENTS 6
	EventNotice *events[NUM_EVENTS];
	double ts[] = {10.0, 3.0, 7.0, 1.0, 5.0, 9.0};

	for (int i = 0; i < NUM_EVENTS; i++) {
		events[i] = make_event(ts[i]);
		fel_insert(&fel, events[i]);
	}

	/* --- Phase 1: Multiple reschedules on the same element --- */
	/* Move event[0] (10.0) to 0.5  → bubble up to root */
	fel_reschedule(&fel, events[0], 0.5);
	/* Move it again to 6.0 → sink down */
	fel_reschedule(&fel, events[0], 6.0);
	/* Move it slightly to 5.5 → small bubble up or down */
	fel_reschedule(&fel, events[0], 5.5);

	/* --- Phase 2: Reschedule to the same timestamp (no net change) --- */
	/* events[2] is at 7.0; move it to 7.0 (should be a no‑op but must keep
	 * heap_index valid) */
	fel_reschedule(&fel, events[2], 7.0);

	/* --- Phase 3: Mixed adjustments, including duplicates --- */
	fel_reschedule(&fel, events[3], 20.0); /* 1.0 → 20.0  (sink deep) */
	fel_reschedule(&fel, events[4], 4.5);  /* 5.0 → 4.5   (small bubble up) */
	fel_reschedule(
	    &fel, events[1],
	    4.5); /* 3.0 → 4.5   (now two events have the same timestamp) */

	/* Check heap_index consistency after all mutations */
	for (int i = 0; i < fel.size; i++) {
		assert(fel.heap[i]->heap_index == i &&
		       "CRITICAL: Event heap_index does not match its actual position "
		       "in the array!");
	}

	/* --- Phase 4: Insert new event after reschedules, then extract mixed ---
	 */
	EventNotice *extra = make_event(2.0);
	fel_insert(&fel, extra);
	/* Reschedule an existing event to the same value as the new one */
	fel_reschedule(&fel, events[5], 2.0); /* 9.0 → 2.0 */

	/* Final extraction: must be monotonic non‑decreasing */
	double prev_timestamp = -1.0;
	int extracted_count = 0;

	while (fel.size > 0) {
		EventNotice *e = fel_extract_min(&fel);
		assert(e->timestamp >= prev_timestamp &&
		       "CRITICAL: Min-heap property violated! Elements extracted out "
		       "of order.");
		prev_timestamp = e->timestamp;
		free_event(e);
		extracted_count++;
	}

	/* We inserted NUM_EVENTS + 1 (the extra) and freed all during extraction */
	assert(extracted_count == NUM_EVENTS + 1 &&
	       "CRITICAL: Lost elements during execution.");

	fel_destroy(&fel);
	printf("PASS\n");
#undef NUM_EVENTS
}

int main(void) {
	test_basic_order();
	test_cancel_skipped();
	test_tie_break_by_id();
	test_peek_empty();
	test_peek_valid();
	test_peek_after_cancel();
	test_cancel_all();
	test_heap_index_after_insert();
	test_heap_index_after_extract();
	test_heap_index_after_cancel_purge();
	test_reschedule_earlier_and_later();
	test_reschedule_heap_index_consistency();

	printf("\nAll Phase 1 tests passed.\n");
	return 0;
}