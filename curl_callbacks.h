#ifndef _TECZKA_CURL_CALLBACKS_H
#define _TECZKA_CURL_CALLBACKS_H

#include <stddef.h>

#include <curl/curl.h>

#include "event.h"

struct teczka_curl_socket_callback_context {
	int epoll_fd;
	struct event_io_curl *event_io_array;
};

// IMPORTANT NOTE: These callbacks SHOULD NOT call libcurl functions. CURL's docs state
// it can lead to recursive behavior.
// EXCEPTION: curl_multi_assign can be called from mulit callbacks. This is useful when the socket
// callback is removing a socket. We'd like to unassociate our pointer with that socket.

// https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
/* Callback function that CURL calls when there is data we must copy to our own buffers.
 * @param data: Pointer to the data CURL needs us to copy. This data is NOT null terminated.
 * @param size: Size of each unit in the data. CURL says this is always one (for now).
 * @param nmemb: Number of units in the data. size * nmemb = total size of data
 * @param event_io_curl_ptr: Pointer the structure tracking CURL event io. The structure contains
 * information about the CURL request and the data that has been read already.
 * given in curl_easy_setopt with CURLOPT_WRITEDATA option.
 * @returns The number of bytes processed by our callback. If that number does not
 * equal size * nmemb, CURL sees that as an error.
 */
size_t teczka_curl_write_callback(char *data, size_t size, size_t nmemb,
				  void *event_io_curl_ptr);

// https://curl.se/libcurl/c/CURLMOPT_TIMERFUNCTION.html
/* Callback function that CURL calls to inform the application curl_multi_socket_action
 * must be called in timeout_ms. CURL's documents dictate only one timer will be active
 * at a time. If this is called when one is active, it should be replaced with the new one.
 * @param mutli_handle: CURL multi interface handle.
 * @param timeout_ms: curl_multi_socket_action should be called in timeout_ms. If this equals
 * -1, the current timer should be deleted.
 * @param event_queue_ptr: Pointer to the event queue which contains CURL operations.
 * @returns 0 on success and -1 on error. If this returns an error, CURL aborts all
 * transfers.
 */
int teczka_curl_timer_callback(CURLM *multi_handle, long timeout_ms,
			       void *event_queue_ptr);

// https://curl.se/libcurl/c/CURLMOPT_SOCKETFUNCTION.html
/* Callback function that CURL calls to inform the application which sockets (file
 * descriptors) should be polled for activity by the application (w/ epoll, select, poll,
 * etc.).
 * @param easy_handle: CURL easy handle. This indicates which transfer the socket is related to.
 * @param socket: The socket (fd) we need to listen on. If what is CURL_POLL_REMOVE, ignore this
 * parameter.
 * @param what: Status of the given socket. This tells the application what to do with socket.
 * @param teczka_curl_socket_callback_context_ptr: Pointer to a structure containing the epoll
 * file descriptor and all the current inflight curl events. This will help us update epoll's
 * state and search for the corresponding easy handle if we haven't bound the curl event to
 * the socket yet.
 * This pointer is given in curl_multi_setopt with CURLMOPT_SOCKETDATA option.
 * @param event_io_curl_ptr: Pointer to the event_io_curl struct associated with the socket socket.
 * We can bind a pointer to a socket with curl_mutli_assign.
 */
int teczka_curl_socket_callback(CURL *easy_handle, curl_socket_t socket,
				int what,
				void *teczka_curl_socket_callback_context_ptr,
				void *event_io_curl_ptr);

#endif // _TECZKA_CURL_CALLBACKS_H
