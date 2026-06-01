#ifndef QSIM_EVENT_INTERNAL_H
#define QSIM_EVENT_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*event_handler_t)(void *context, void *data);

typedef struct EventNotice {
	uint64_t id;
	double timestamp;
	event_handler_t handler;
	void *data;
	bool valid;
	int heap_index;
} EventNotice;

#endif