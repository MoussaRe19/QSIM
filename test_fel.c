#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>
#include "fel.h"


static uint64_t next_id = 1;

static EventNotice *make_event(double timestamp) {
    EventNotice *e = (EventNotice*)malloc(sizeof(EventNotice));
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

    for(int i = 0; i < 5; i++) {
        events[i] = make_event(timestamps[i]);
        fel_insert(&fel, events[i]);
    }

    double expected[] = {1.0, 2.0, 3.0, 5.0, 8.0};
    for(int i = 0; i < 5; i++) {
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

    EventNotice *r1 = fel_extract_min(&fel); 
    assert(r1 != NULL && r1->timestamp == 1.0);

    EventNotice *r2 = fel_extract_min(&fel); 
    assert(r2 != NULL && r2->timestamp == 3.0);

    assert(fel_extract_min(&fel) == NULL);

    free_event(e1);
    free_event(e2);
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

    assert(fel_peek(&fel) == 2.0);

    free_event(e1);
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
    fel_cancel(e2);

    assert(isinf(fel_peek(&fel)));
    assert(fel_extract_min(&fel) == NULL);

    free_event(e1); 
    free_event(e2);
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

    for(int i = 0; i < fel.size; i++) {
        assert(fel.heap[i]->heap_index == i);
    }

    free_event(e1);
    free_event(e2);
    free_event(e3);
    fel_destroy(&fel);
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
    free_event(e1); 
    free_event(e3);

    fel_destroy(&fel);
    printf("PASS\n");
}

static void test_heap_index_after_cacel_purge(void) {
    printf("test_heap_index_after_cancel_purge ... ");
    FEL fel;
    fel_init(&fel);

    EventNotice *e1 = make_event(1.0);
    EventNotice *e2 = make_event(2.0);
    fel_insert(&fel, e1);
    fel_insert(&fel, e2);

    fel_cancel(e1);
    fel_peek(&fel);

    assert(e1->heap_index == -1);
    assert(fel.heap[0]->heap_index == 0);

    free_event(e1);
    free_event(fel_extract_min(&fel));
    fel_destroy(&fel);
    printf("PASS\n");
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
    test_heap_index_after_cacel_purge();

    printf("\nAll Phase 1 tests passed.\n");
    return 0;
}