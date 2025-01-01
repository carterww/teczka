#ifndef _TECZKA_EVENT_LOOP_H
#define _TECZKA_EVENT_LOOP_H

#include <sys/epoll.h>

#include "event.h"
#include "portfolio.h"

// Specify what we want to poll for. These are used in the actions_flag
// bitmask param for event_loop_fd_* functions.
#define EVENT_LOOP_FD_POLL_IN (EPOLLIN)
#define EVENT_LOOP_FD_POLL_OUT (EPOLLOUT)

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

enum event_loop_fd_addmod_error {
	EVENT_LOOP_FD_ADDMOD_ERROR_OK = 0,
	EVENT_LOOP_FD_ADDMOD_ERROR_INVALID_FD,
};

enum event_loop_fd_del_error {
	EVENT_LOOP_FD_DEL_ERROR_OK = 0,
	EVENT_LOOP_FD_DEL_ERROR_INVALID_FD,
};

enum event_loop_init_error event_loop_init(void);
void event_loop_start(struct event_loop_context *context);

/* Adds or modifies information associated with the file descriptor.
 * @param fd: File descriptor to listen to.
 * If you don't specify a flag then that arg is ignored and unchanged.
 * @param actions_flag: Bitmask of actions the user would like the event loop to listen
 * for. The current actions are:
 *   - EVENT_LOOP_FD_POLL_IN: Listen for read operations
 *   - EVENT_LOOP_FD_POLL_OUT: Listen for write operations
 * @param event: The event_io_curl structure associated with that fd. This will be returned
 * by the poll function (epoll only for now) when there is activity. This allows us to
 * identify which event progressed.
 * @returns an enum indicating whether an error occurred
 */
enum event_loop_fd_addmod_error
event_loop_fd_addmod(int fd, uint32_t actions_flag,
		     struct event_io_curl *event);

/* Removes the file descriptor from the event loop's listen list.
 * @param fd: File descriptor to listen to.
 * @returns an enum indicating whether an error occurred
 */
enum event_loop_fd_del_error event_loop_fd_del(int fd);

#endif // _TECZKA_EVENT_LOOP_H
