#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "curl_callbacks.h"
#include "event.h"
#include "util.h"

// REMINDER: DO NOT CALL CURL FUNCTIONS FROM THESE!!!

size_t teczka_curl_write_callback(char *data, size_t size, size_t nmemb,
				  void *my_ptr)
{
	return 0;
}

int teczka_curl_timer_callback(CURLM *multi_handle, long timeout_ms,
			       void *event_queue_ptr)
{
	uint64_t now_ms = timestamp_ms_get();
	if (NULL == event_queue_ptr) {
		printf("No user defined pointer passed to teczka_curl_timer_callback. "
		       "This could be due to a CURL bug or curl_multi_setopt was never called "
		       "with CURLMOPT_TIMERDATA.\n");
		// This is a huge issue. We should return error
		return -1;
	}
	struct event_queue *queue = (struct event_queue *)event_queue_ptr;
	struct event *curl_timeout_event = &queue->curl_timeout_event;
	switch (curl_timeout_event->tag) {
	case TECZKA_EVENT_CURL_TIMEOUT:
		break;
	case TECZKA_EVENT_FETCH_STOCK:
	case TECZKA_EVENT_DISPLAY_STOCK:
	case TECZKA_EVENT_DISPLAY_PORTFOLIO:
	default:
		printf("event_queue's curl_timeout_event does not have the correct tag %d\n",
		       curl_timeout_event->tag);
		return -1;
	}
	// Update the timeout of the current curl_timeout event. If timeout_ms >= 0, add
	// now_ms to timeout_ms to get the time event_loop should schedule it.
	curl_timeout_event->run_timestamp_ms = timeout_ms;
	if (timeout_ms >= 0) {
		curl_timeout_event->run_timestamp_ms += now_ms;
	}
	return 0;
}

int teczka_curl_socket_callback(CURL *easy_handle, curl_socket_t socket,
				int what,
				void *teczka_curl_socket_callback_context_ptr,
				void *event_io_curl_ptr)
{
	return -1;
}
