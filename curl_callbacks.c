#include <curl/multi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "config.h"
#include "curl_callbacks.h"
#include "event.h"
#include "event_loop.h"
#include "util.h"

static struct event_io_curl *
_event_io_find(CURL *easy_handle, struct event_io_curl event_io_array[],
	       size_t array_len);

static int _curl_poll_remove(curl_socket_t socket,
			     struct event_io_curl *event_io);

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
	// Important note: the easy_handle is not always one of ours. CURL could pass an
	// internal easy handle. In this case, there is no associated event_io_curl struct.
	if (NULL == easy_handle) {
		printf("teczka_curl_socket_callback received a NULL easy handle.\n");
		return -1;
	}
	if (NULL == teczka_curl_socket_callback_context_ptr) {
		printf("teczka_curl_socket_callback received a NULL context ptr. "
		       "Make sure CURLMOPT_SOCKETDATA was set.\n");
		return -1;
	}
	// context is not null.
	struct teczka_curl_socket_callback_context *context =
		(struct teczka_curl_socket_callback_context *)
			teczka_curl_socket_callback_context_ptr;
	// event_io_passed is null if we haven't bound it to the socket yet.
	struct event_io_curl *event_io_passed =
		(struct event_io_curl *)event_io_curl_ptr;
	struct event_io_curl *event_io_found = NULL;
	// If event_io is null, let's try to find it to bind it to the socket.
	// event_io_found can be null if the easy_handle is internal to CURL. That
	// case is ok because we don't need to track socket in our data structures
	if (NULL == event_io_passed) {
		event_io_found = _event_io_find(easy_handle,
						context->event_io_array,
						EVENT_IO_CURL_BUFFER_LEN);
	}

	if (CURL_POLL_REMOVE == what) {
		// Prefer passed one. Fallback to found one.
		struct event_io_curl *event_io_to_remove = event_io_passed;
		if (NULL == event_io_passed) {
			event_io_to_remove = event_io_found;
		} else {
			// If we had an event bound to socket, unbind it
			(void)curl_multi_assign(context->multi_handle, socket,
						NULL);
		}
		(void)_curl_poll_remove(socket, event_io_to_remove);
		return 0;
	}
	uint32_t event_loop_action_flags = 0;
	struct event_io_curl *event_io_bound_socket = event_io_passed;
	// Now we know our op should be to add or modify the actions to listen for on socket.
	if (CURL_POLL_IN & what) {
		event_loop_action_flags |= EVENT_LOOP_FD_POLL_IN;
	}
	if (CURL_POLL_OUT & what) {
		event_loop_action_flags |= EVENT_LOOP_FD_POLL_OUT;
	}

	if (NULL != event_io_passed) {
		event_io_passed->sockfd = socket;
	} else {
		// If this is the case we need to bind event to socket
		if (NULL != event_io_found) {
			event_io_bound_socket = event_io_found;
			event_io_found->sockfd = socket;
			(void)curl_multi_assign(context->multi_handle, socket,
						event_io_found);
		}
	}
	(void)event_loop_fd_addmod(socket, event_loop_action_flags,
				   event_io_bound_socket);

	return 0;
}

static struct event_io_curl *
_event_io_find(CURL *easy_handle, struct event_io_curl event_io_array[],
	       size_t array_len)
{
	for (size_t i = 0; i < array_len; ++i) {
		if (easy_handle == event_io_array[i].easy_handle) {
			return &event_io_array[i];
		}
	}
	return NULL;
}

static int _curl_poll_remove(curl_socket_t socket,
			     struct event_io_curl *event_io)
{
	if (NULL != event_io) {
		event_io->sockfd = -1;
	}
	enum event_loop_fd_del_error del_result = event_loop_fd_del(socket);
	switch (del_result) {
	case EVENT_LOOP_FD_DEL_ERROR_OK:
		return 0;
	case EVENT_LOOP_FD_DEL_ERROR_INVALID_FD:
	default:
		return -1;
	}
}
