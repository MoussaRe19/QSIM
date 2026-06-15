#ifndef ENTITY_QUEUE_H
#define ENTITY_QUEUE_H

#include "entity.h"

typedef struct EntityNode {
	Entity *entity;
	struct EntityNode *next;
} EntityNode;

typedef struct {
	EntityNode *head;
	EntityNode *tail;
	int size;
} EntityQueue;

void entity_queue_init(EntityQueue *q);
void entity_queue_push(EntityQueue *q, Entity *e);
Entity *entity_queue_pop(EntityQueue *q);
void entity_queue_destroy(EntityQueue *q);

#endif