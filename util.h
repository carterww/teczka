#ifndef _TECZKA_UTIL_H
#define _TECZKA_UTIL_H

#include <stddef.h>
#include <stdint.h>

struct data_buffer {
	char *buffer;
	size_t buffer_size_bytes;
	size_t buffer_used_bytes;
};

/* Returns a timestamp in milliseconds from some unspecified staring point.
 * The timestamps returned by this function only move forward and will not
 * have large shifts in time between consecutive calls.
 * The timestamp does NOT correspond to calendar time (it's not time since epoch).
 */
uint64_t timestamp_ms_get(void);

void sleep_ms(uint64_t ms);

#endif // _TECZKA_UTIL_H
