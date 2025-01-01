#include <errno.h>
#include <stdlib.h>
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
// Curl may require our application to listen to sockets that are internal to CURL. This
// means the event_io_inflight array may not contain all of the sockets epoll is actually
// listening to. We could epoll_ctl add on each socket callback and retry with mod if
// epoll returns EEXIST but then we are doing 2 syscalls. It would be much faster to just
// linearly search an array that fits in a couple cache lines.
static int epoll_fd_arr[EVENT_LOOP_FDS_MAX] = { -1 };

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

// Internal functions
static enum event_loop_init_error
_curl_init(struct event_io_curl *event_io_array);
static int _curl_socket_setopts(void);
static int _curl_timer_setopts(void);
static void _curl_cleanup(void);

static enum event_loop_init_error _epoll_init(void);
static void _epoll_cleanup(void);
static uint32_t _event_loop_action_flags_to_epoll_events(uint32_t action_flags);

static void _epoll_fd_arr_del(int fd);
static void _epoll_fd_arr_add(int fd);
static int _epoll_fd_arr_has(int fd);

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
		_curl_init(event_io_inflight);
	if (EVENT_LOOP_INIT_ERROR_OK != curl_init_result) {
		return curl_init_result;
	}

	return EVENT_LOOP_INIT_ERROR_OK;
}

void event_loop_start(struct event_loop_context *context);

enum event_loop_fd_addmod_error
event_loop_fd_addmod(int fd, uint32_t actions_flag, struct event_io_curl *event)
{
	struct epoll_event epoll_ev = {
		.data = { .ptr = (void *)event },
		.events = _event_loop_action_flags_to_epoll_events(actions_flag)
	};
	const int already_listening = _epoll_fd_arr_has(fd);
	const int epoll_ctl_op = already_listening ? EPOLL_CTL_MOD :
						     EPOLL_CTL_ADD;
	const int epoll_ctl_addmod_result =
		epoll_ctl(epoll_fd, epoll_ctl_op, fd, &epoll_ev);
	if (0 == epoll_ctl_addmod_result) {
		if (!already_listening) {
			_epoll_fd_arr_add(fd);
		}
		return EVENT_LOOP_FD_ADDMOD_ERROR_OK;
	}
	const char *epoll_ctl_op_str = already_listening ? "EPOLL_CTL_MOD" :
							   "EPOLL_CTL_ADD";
	printf("epoll_ctl (%s) failed with errno %d\n", epoll_ctl_op_str,
	       errno);
	switch (errno) {
	case ENOENT:
		printf("event_loop_fd_addmod attempted to modify a fd not registered with epoll.\n");
		return EVENT_LOOP_FD_ADDMOD_ERROR_INVALID_FD;
	case EEXIST:
		printf("event_loop_fd_addmod attempted to add a fd already registered with epoll.\n");
		return EVENT_LOOP_FD_ADDMOD_ERROR_INVALID_FD;
	case EBADF:
	case EINVAL:
	case ELOOP:
	case ENOMEM:
	case ENOSPC:
	case EPERM:
	default:
		goto unrecoverable;
	}
unrecoverable:
	printf("event_loop_fd_addmod encounterd an unrecoverable error. Exitting now\n");
	exit(1);
}

enum event_loop_fd_del_error event_loop_fd_del(int fd)
{
	if (epoll_fd < 0) {
		printf("event_loop_fd_del was called before the event loop was initialized.");
		// I could call init here but eh. I don't like hidden behavior like that
		goto unrecoverable;
	}
	// Docs state epoll_ctl ignores the epoll_event arg for op EPOLL_CTL_DEL after
	// Linux 2.6.9. We will specify it anyway for portability.
	struct epoll_event epoll_ev;
	int epoll_del_result =
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &epoll_ev);
	if (0 == epoll_del_result) {
		_epoll_fd_arr_del(fd);
		return EVENT_LOOP_FD_DEL_ERROR_OK;
	}
	// Error occurred, figure out what to do
	printf("epoll_ctl (EPOLL_CTL_DEL) failed with errno %d\n", errno);
	switch (errno) {
	case ENOENT:
		printf("event_loop_fd_del attempted to delete a fd not registered with epoll.\n");
		return EVENT_LOOP_FD_DEL_ERROR_INVALID_FD;
	case EBADF:
	case EEXIST:
	case EINVAL:
	case ELOOP:
	case ENOMEM:
	case ENOSPC:
	case EPERM:
	default:
		goto unrecoverable;
	}
unrecoverable:
	printf("event_loop_fd_del encounterd an unrecoverable error. Exitting now\n");
	exit(1);
}

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
_curl_init(struct event_io_curl *event_io_array)
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
		.multi_handle = curl_multi_handle,
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

static uint32_t _event_loop_action_flags_to_epoll_events(uint32_t action_flags)
{
	// These are the same right now. I just wanted to wrap this in a function
	// to remind myself of this behavior
	return action_flags;
}

static void _epoll_fd_arr_del(int fd)
{
	for (size_t i = 0; i < EVENT_LOOP_FDS_MAX; ++i) {
		if (fd == epoll_fd_arr[i]) {
			epoll_fd_arr[i] = -1;
			break;
		}
	}
}

static void _epoll_fd_arr_add(int fd)
{
	for (size_t i = 0; i < EVENT_LOOP_FDS_MAX; ++i) {
		if (-1 == epoll_fd_arr[i]) {
			epoll_fd_arr[i] = fd;
			break;
		}
	}
}

static int _epoll_fd_arr_has(int fd)
{
	for (size_t i = 0; i < EVENT_LOOP_FDS_MAX; ++i) {
		if (fd == epoll_fd_arr[i]) {
			return 1;
		}
	}
	return 0;
}
