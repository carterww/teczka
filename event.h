#ifndef _TECZKA_EVENT_H
#define _TECZKA_EVENT_H

#include "util.h"
#include <stdint.h>

#include <curl/curl.h>

#include "equity.h"
#include "kette.h"
#include "portfolio.h"

enum event_tag {
	TECZKA_EVENT_FETCH_STOCK,
	TECZKA_EVENT_DISPLAY_STOCK,
	TECZKA_EVENT_DISPLAY_PORTFOLIO,
	TECZKA_EVENT_CURL_TIMEOUT,
};

struct event_stock_fetch {
	struct equity *stock;
};

struct event_stock_display {
	struct equity *stock;
	struct equity *stock_next; // For queuing next stock display
};

struct event_portfolio_display {
	struct portfolio *portfolio;
};

struct event_curl_timeout {
	CURLM *multi_handle;
};

// Tagged union of events and the time they should be executed.
struct event {
	enum event_tag tag;
	uint64_t run_timestamp_ms; // When event needs to be executed
	union {
		struct event_stock_fetch stock_fetch_info;
		struct event_stock_display stock_display_info;
		struct event_portfolio_display portfolio_display_info;
		struct event_curl_timeout curl_timeout_info;
	};
};

struct event_io_curl {
	CURL *easy_handle;
	curl_socket_t sockfd;
	struct event *event;
	struct data_buffer buffer;
};

struct event_node {
	struct slink link;
	struct event event;
};

struct event_queue {
	struct slink event_head;
	// There can only be one outstanding curl_timeout so we'll keep it here.
	// We need to check this often anyway.
	struct event curl_timeout_event;
};

int event_queue_init(struct event_queue *queue);
int event_queue_add(struct event_queue *queue, struct event_node *event);
const struct event_node *event_queue_dequeue(struct event_queue *queue);
const struct event_node *event_queue_peek(struct event_queue *queue);

#endif // _TECZKA_EVENT_H
