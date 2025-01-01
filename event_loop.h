#ifndef _TECZKA_EVENT_LOOP_H
#define _TECZKA_EVENT_LOOP_H

#include "portfolio.h"

struct event_loop_context {
	struct portfolio *portfolio;
};

enum event_loop_init_error {
	EVENT_LOOP_INIT_ERROR_OK = 0,
	EVENT_LOOP_INIT_ERROR_EPOLL_FAIL,
	EVENT_LOOP_INIT_ERROR_CURL_GLOBAL_FAIL,
	EVENT_LOOP_INIT_ERROR_CURL_MULTI_FAIL,
	EVENT_LOOP_INIT_ERROR_CURL_SETOPT_FAIL,
	EVENT_LOOP_INIT_ERROR_EVENT_CACHE_FAIL,
	EVENT_LOOP_INIT_ERROR_EVENT_QUEUE_FAIL,
};

enum event_loop_init_error event_loop_init(void);
void event_loop_start(struct event_loop_context *context);

#endif // _TECZKA_EVENT_LOOP_H
