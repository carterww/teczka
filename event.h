#ifndef _TECZKA_EVENT_H
#define _TECZKA_EVENT_H

#include <stdint.h>

#include "kette.h"

struct event {
	int64_t priority;
	void (*event_handler)(void *arg);
	void *const arg;
};

struct event_node {
	struct slink link;
	struct event event;
};

struct event_queue {
	struct slink event_head;
};

int event_queue_init(struct event_queue *queue);
int event_queue_add(struct event_queue *queue, struct event_node *event);
const struct event_node *event_queue_dequeue(struct event_queue *queue);
const struct event_node *event_queue_peek(struct event_queue *queue);

#endif // _TECZKA_EVENT_H
