// This is not good but I need monotonic time
#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <time.h>

#include "util.h"

#define MS_PER_SEC (1000)
#define NS_PER_MS (1000000)

static inline uint64_t timespec_to_ms(const struct timespec *ts)
{
	return (uint64_t)ts->tv_sec * MS_PER_SEC +
	       (uint64_t)ts->tv_nsec / NS_PER_MS;
}

// NOT TIME SINCE EPOCH. DON'T EXPECT THAT
uint64_t get_timestamp_ms(void)
{
	struct timespec ts;
	// CLOCK_BOOTTIME is preferred here because it is not
	// affected by NTP (unlike CLOCK_REALTIME) and includes
	// suspend time (unlike CLOCK_MONOTONIC). For my purposes, this does
	// not have to correspond to the time since epoch.
	(void)clock_gettime(CLOCK_BOOTTIME, &ts);
	return timespec_to_ms(&ts);
}

#undef NS_PER_MS
#undef MS_PER_SEC
#undef _POSIX_C_SOURCE
