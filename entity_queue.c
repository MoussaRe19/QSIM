#include <assert.h>
#include <stdlib.h>
#include "entity_queue.h"

void entity_queue_init(EntityQueue *q) {
	q->head = NULL;
	q->tail = NULL;
	q->size = 0;
}

void entity_queue_push(EntityQueue *q, Entity *e) {
	EntityNode *node = malloc(sizeof(EntityNode));
	assert(node != NULL && "entity_queue_push: allocation failed");
	node->entity = e;
	node->next = NULL;
	if (q->tail)
		q->tail->next = node;
	else
		q->head = node;

	q->tail = node;
	q->size++;
}

Entity *entity_queue_pop(EntityQueue *q) {
	assert(q->head != NULL && "entity_queue_pop: queue is empty");
	EntityNode *node = q->head;
	Entity *e = node->entity;
	q->head = node->next;
	if (!q->head) q->tail = NULL;

	free(node);
	q->size--;
	return e;
}

void entity_queue_destroy(EntityQueue *q) {
	while (q->head) {
		EntityNode *node = q->head;
		q->head = node->next;
		entity_destroy(node->entity);
		free(node);
	}

	q->tail = NULL;
	q->size = 0;
}