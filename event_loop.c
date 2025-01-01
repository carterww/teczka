#include <errno.h>

#include <sys/epoll.h>

#include "config.h"
#include "event.h"
#include "event_loop.h"

// Being precise for the curl_timeout event is important. We'll keep a struct for
// tracking the max time each event takes to run. If curl_timeout needs to be handled
// before the highest priority event will finish (just a guess), then we need to know.
struct event_runtime_max_ms {
	uint64_t stock_fetch;
	uint64_t stock_display;
	uint64_t portfolio_display;
};

/* There can only be one event loop in the applicaton because it is single threaded.
 * Because of this, I have decided to make all the event loop variables static
 * globals in this file. I generally dislike using global variables but I think in
 * this case it makes the most sense.
 */
static int epoll_fd = -1;
static struct epoll_event epoll_wait_events[EVENT_LOOP_EPOLL_EVENTS_LEN];

static struct event_io_curl event_io_inflight[EVENT_IO_CURL_BUFFER_LEN];

static struct event_runtime_max_ms runtimes =
	(struct event_runtime_max_ms){ 0 };

enum event_loop_init_error event_loop_init(void)
{
	// Docs state size param has been ignored since Linux 2.6.8. We'll specify
	// one for portability.
	epoll_fd = epoll_create(EVENT_LOOP_EPOLL_SIZE);
	if (epoll_fd < 0) {
		printf("epoll_create failed with errno %d.\n", errno);
		return EVENT_LOOP_INIT_ERROR_EPOLL_FAIL;
	}

	return EVENT_LOOP_INIT_ERROR_OK;
}

void event_loop_start(struct event_queue *queue);
