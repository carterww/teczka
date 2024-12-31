#include <stddef.h>
#include <stdint.h>

#include "event.h"
#include "kette.h"

int event_queue_init(struct event_queue *queue)
{
	if (NULL == queue) {
		return 1;
	}
	slist_init(&queue->event_head);
	return 0;
}

int event_queue_add(struct event_queue *queue, struct event_node *event)
{
	if (NULL == queue || NULL == event) {
		return 1;
	}
	struct event_node *prev = NULL;
	struct event_node *curr =
		list_entry(queue->event_head.next, struct event_node, link);
	// queue->event_head is a "pseudo" head (I don't know proper term for this).
	// It doesn't actually hold a value, it's just a link.
	while (&curr->link != &queue->event_head) {
		// If the event to add should run sooner then we stop.
		if (event->event.run_timestamp_ms <
		    curr->event.run_timestamp_ms) {
			break;
		}
		prev = curr;
		curr = list_entry(curr->link.next, struct event_node, link);
	}

	// If prev is NULL then there are no events in the queue. We add it directly
	// after the pseudo head. Otherwise we add it after prev.
	if (NULL == prev) {
		slist_add(&event->link, &queue->event_head);
	} else {
		slist_add(&event->link, &prev->link);
	}

	return 0;
}

const struct event_node *event_queue_dequeue(struct event_queue *queue)
{
	if (NULL == queue || list_empty(&queue->event_head)) {
		return NULL;
	}
	const struct event_node *head =
		list_entry(queue->event_head.next, struct event_node, link);
	slist_del(queue->event_head.next);
	return head;
}

const struct event_node *event_queue_peek(struct event_queue *queue)
{
	if (NULL == queue || list_empty(&queue->event_head)) {
		return NULL;
	}
	return list_entry(queue->event_head.next, struct event_node, link);
}
