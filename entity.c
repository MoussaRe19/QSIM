#include <assert.h>
#include <stdlib.h>
#include "entity.h"

static uint64_t entity_id_ctr = 0;

Entity *entity_create(double arrival_time) {
	Entity *e = malloc(sizeof(Entity));
	assert(e != NULL && "entity_create: allocation failed");
	e->id = entity_id_ctr++;
	e->arrival_time = arrival_time;
	e->service_start_time = -1.0;
	return e;
}

void entity_destroy(Entity *e) {
	free(e);
}