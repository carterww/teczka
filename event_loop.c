#include <errno.h>
#include <unistd.h>

#include <curl/multi.h>
#include <sys/epoll.h>

#include "config.h"
#include "curl_callbacks.h"
#include "event.h"
#include "event_loop.h"
#include "static_mem_cache.h"

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
static CURLM *curl_multi_handle = NULL;

// Setting this to 0 because I'm unsure what epoll_wait expects. This is the safest
// option.
static struct epoll_event epoll_wait_events[EVENT_LOOP_EPOLL_EVENTS_LEN] = { 0 };
// Setting this to 0 because this ensures the application will see they are all empty
// on init.
static struct event_io_curl event_io_inflight[EVENT_IO_CURL_BUFFER_LEN] = { 0 };
static struct event_node EVENT_NODE_STATIC_BUFFER[MEM_CACHE_EVENT_NODE_COUNT];
static struct static_mem_cache event_node_cache;
static struct event_queue event_queue;

static struct teczka_curl_socket_callback_context socket_callback_context;

static struct event_runtime_max_ms runtimes = { 0 };

static enum event_loop_init_error _queue_init(void);

static enum event_loop_init_error
_curl_init(int epoll_file_desc, struct event_io_curl *event_io_array);
static int _curl_socket_setopts(void);
static int _curl_timer_setopts(void);
static void _curl_cleanup(void);

static enum event_loop_init_error _epoll_init(void);
static void _epoll_cleanup(void);

enum event_loop_init_error event_loop_init(void)
{
	enum event_loop_init_error queue_init_result = _queue_init();
	if (EVENT_LOOP_INIT_ERROR_OK != queue_init_result) {
		return queue_init_result;
	}
	enum event_loop_init_error epoll_init_res = _epoll_init();
	if (EVENT_LOOP_INIT_ERROR_OK != epoll_init_res) {
		return epoll_init_res;
	}
	// Initializing curl depends on the previous two. This is because we set some options
	// in CURL that require user data pointers.
	enum event_loop_init_error curl_init_result =
		_curl_init(epoll_fd, event_io_inflight);
	if (EVENT_LOOP_INIT_ERROR_OK != curl_init_result) {
		return curl_init_result;
	}

	return EVENT_LOOP_INIT_ERROR_OK;
}

void event_loop_start(struct event_loop_context *context);

static enum event_loop_init_error _queue_init(void)
{
	const int event_node_cache_init_res = static_mem_cache_init(
		&event_node_cache, EVENT_NODE_STATIC_BUFFER,
		MEM_CACHE_EVENT_NODE_COUNT, sizeof(struct event_node),
		STATIC_MEM_CACHE_FLAG_CHECK_FREE_LIST_ON_FREE);
	if (STATIC_MEM_CACHE_INIT_ERROR_OK != event_node_cache_init_res) {
		printf("Failed to initialize the event static mem cache with result %d\n",
		       event_node_cache_init_res);
		return EVENT_LOOP_INIT_ERROR_EVENT_CACHE_FAIL;
	}
	const int event_queue_init_res = event_queue_init(&event_queue);
	if (event_queue_init_res) {
		printf("Failed to initialize the event queue\n");
		return EVENT_LOOP_INIT_ERROR_EVENT_QUEUE_FAIL;
	}
	return EVENT_LOOP_INIT_ERROR_OK;
}

static enum event_loop_init_error
_curl_init(int epoll_file_desc, struct event_io_curl *event_io_array)
{
	CURLcode curl_init_result =
		curl_global_init(CURL_GLOBAL_SSL | CURL_GLOBAL_ACK_EINTR);
	if (CURLE_OK != curl_init_result) {
		printf("curl_global_init failed with code %d\n",
		       curl_init_result);
		return EVENT_LOOP_INIT_ERROR_CURL_GLOBAL_FAIL;
	}
	curl_multi_handle = curl_multi_init();
	if (NULL == curl_multi_handle) {
		printf("curl_multi_init failed\n");
		return EVENT_LOOP_INIT_ERROR_CURL_MULTI_FAIL;
	}
	socket_callback_context = (struct teczka_curl_socket_callback_context){
		.epoll_fd = epoll_file_desc,
		.event_io_array = event_io_array,
	};
	int setopt_aggregate_result = _curl_socket_setopts();
	setopt_aggregate_result = setopt_aggregate_result ||
				  _curl_timer_setopts();
	if (setopt_aggregate_result) {
		return EVENT_LOOP_INIT_ERROR_CURL_SETOPT_FAIL;
	}

	return EVENT_LOOP_INIT_ERROR_OK;
}

static int _curl_socket_setopts(void)
{
	CURLMcode socket_funtion_opt_result;
	CURLMcode socket_data_opt_result;

	socket_funtion_opt_result =
		curl_multi_setopt(curl_multi_handle, CURLMOPT_SOCKETFUNCTION,
				  teczka_curl_socket_callback);
	if (CURLM_OK != socket_funtion_opt_result) {
		printf("curl_multi_setopt CURLMOPT_SOCKETFUNCTION failed with curlm code %d\n",
		       socket_funtion_opt_result);
		return 1;
	}
	socket_data_opt_result =
		curl_multi_setopt(curl_multi_handle, CURLMOPT_SOCKETDATA,
				  (void *)&socket_callback_context);
	if (CURLM_OK != socket_data_opt_result) {
		printf("curl_multi_setopt CURLMOPT_SOCKETDATA failed with curlm code %d\n",
		       socket_funtion_opt_result);
		return 1;
	}
	return 0;
}

static int _curl_timer_setopts(void)
{
	CURLMcode timer_function_opt_result;
	CURLMcode timer_data_opt_result;

	timer_function_opt_result =
		curl_multi_setopt(curl_multi_handle, CURLMOPT_TIMERFUNCTION,
				  teczka_curl_timer_callback);
	if (CURLM_OK != timer_function_opt_result) {
		printf("curl_mutli_setopt CURLMOPT_TIMERFUNCTION failed with curlm code %d\n",
		       timer_function_opt_result);
		return 1;
	}

	timer_data_opt_result = curl_multi_setopt(
		curl_multi_handle, CURLMOPT_TIMERDATA, (void *)&event_queue);
	if (CURLM_OK != timer_data_opt_result) {
		printf("curl_mutli_setopt CURLMOPT_TIMERDATA failed with curlm code %d\n",
		       timer_data_opt_result);
		return 1;
	}
	return 0;
}

static void _curl_cleanup(void)
{
	if (NULL != curl_multi_handle) {
		CURLMcode multi_cleanup_result =
			curl_multi_cleanup(curl_multi_handle);
		if (CURLM_OK != multi_cleanup_result) {
			printf("curl_multi_cleanup failed with code %d\n",
			       multi_cleanup_result);
		}
	}
	curl_global_cleanup();
}

static enum event_loop_init_error _epoll_init(void)
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

static void _epoll_cleanup(void)
{
	if (epoll_fd >= 0) {
		int close_result = close(epoll_fd);
		if (0 != close_result) {
			printf("close failed to close epoll_fd with errno %d.\n",
			       errno);
		}
	}
}
