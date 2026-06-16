#ifndef QSIM_FEL_INTERNAL_H
#define QSIM_FEL_INTERNAL_H

#include "event.h"

#define FEL_INITIAL_CAPACITY 64
#define FEL_GROWTH_FACTOR 2

typedef struct {
	EventNotice **heap;
	int size;
	int capacity;
} FEL;

void fel_init(FEL *fel);
void fel_destroy(FEL *fel);

void fel_insert(FEL *fel, EventNotice *e);
EventNotice *fel_extract_min(FEL *fel);
double fel_peek(FEL *fel);
void fel_cancel(EventNotice *e);

void fel_reschedule(FEL *fel, EventNotice *e, double new_timestamp);
double fel_peek_timestamp_dispatch(FEL *fel, double tau_max);

#endif