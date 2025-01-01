#ifndef _TECZKA_EVENT_LOOP_H
#define _TECZKA_EVENT_LOOP_H

#include "event.h"

enum event_loop_init_error {
	EVENT_LOOP_INIT_ERROR_OK = 0,
	EVENT_LOOP_INIT_ERROR_EPOLL_FAIL,
};

enum event_loop_init_error event_loop_init(void);
void event_loop_start(struct event_queue *queue);

#endif // _TECZKA_EVENT_LOOP_H
