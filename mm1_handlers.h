#ifndef MM1_HANDLERS_H
#define MM1_HANDLERS_H

/* Event handlers for M/M/1 queue. */

void handler_arrival(void *context, void *data);
void handler_departure(void *context, void *data);

#endif