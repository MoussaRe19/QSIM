#ifndef ENTITY_H
#define ENTITY_H

#include <stdint.h>

typedef struct {
	uint64_t id;
	double arrival_time;
	double service_start_time;
} Entity;

Entity *entity_create(double arrival_time);
void entity_destroy(Entity *e);

#endif