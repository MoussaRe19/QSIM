#include "fel.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>


static void swap(FEL *fel, int i, int j) {
    EventNotice *tmp = fel->heap[i];
    fel->heap[i] = fel->heap[j]; 
    fel->heap[j] = tmp;

    fel->heap[i]->heap_index = i; 
    fel->heap[j]->heap_index = j;
}

static bool a_less_then_b(const EventNotice *a, const EventNotice *b) {
    if(a->timestamp < b->timestamp) return true;
    if(a->timestamp > b->timestamp) return false;

    return a->id < b->id;
}

static void sift_up(FEL *fel, int idx) {
    while(idx > 0) {
        int parent = (idx - 1) / 2; 
        if(a_less_then_b(fel->heap[idx], fel->heap[parent])) {
            swap(fel, idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}


static void sift_down(FEL *fel, int idx) {
    int size = fel->size;
    while(1) {
        int left = 2 * idx + 1; 
        int right = 2 * idx + 2; 
        int smallest = idx;

        if(left < size && a_less_then_b(fel->heap[left], fel->heap[smallest]))
            smallest = left;

        if(right < size && a_less_then_b(fel->heap[right], fel->heap[smallest]))
            smallest = right;

        if(smallest != idx) {
            swap(fel, idx, smallest);
            idx = smallest;
        } else {
            break;
        }
    }
}

// Public API

void fel_init(FEL *fel) {
    fel->capacity = FEL_INITIAL_CAPACITY;
    fel->size = 0; 
    fel->heap = (EventNotice**)malloc(fel->capacity * sizeof(EventNotice*));
    assert(fel->heap != NULL);    
}

void fel_destroy(FEL *fel) {
    free(fel->heap); 
    fel->heap = NULL;
    fel->size = 0; 
    fel->capacity = 0;
}

void fel_insert(FEL *fel, EventNotice *e) {
    if(fel->size == fel->capacity) {
        int new_capacity = fel->capacity * FEL_GROWTH_FACTOR;
        EventNotice **new_heap = (EventNotice **)realloc(fel->heap, new_capacity * sizeof(EventNotice *));
        assert(new_heap != NULL);
        fel->heap = new_heap;
        fel->capacity = new_capacity;
    }
    e->heap_index = fel->size;
    fel->heap[fel->size] = e;
    fel->size++;
    sift_up(fel, e->heap_index);
}

EventNotice *fel_extract_min(FEL *fel) {
    while(fel->size > 0) {
        EventNotice *root = fel->heap[0];
        fel->size--;
        if (fel->size > 0) {
            fel->heap[0] = fel->heap[fel->size];
            fel->heap[0]->heap_index = 0;
            sift_down(fel, 0);
        } else {
            fel->heap[0] = NULL;
        }
        root->heap_index = -1;
        if (root->valid) return root;
    }
    return NULL;
}

double fel_peek(FEL *fel) {
    while(fel->size > 0 && !fel->heap[0]->valid) {
        EventNotice *root = fel->heap[0];
        fel->size--;
        if (fel->size > 0) {
            fel->heap[0] = fel->heap[fel->size];
            fel->heap[0]->heap_index = 0;
            sift_down(fel, 0);
        } else {
            fel->heap[0] = NULL;
        }
        root->heap_index = -1;
    }
    return (fel->size > 0) ? fel->heap[0]->timestamp : INFINITY;
}

void fel_cancel(EventNotice *e) {
    e->valid = false;
}