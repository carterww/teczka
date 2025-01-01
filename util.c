// This is not good but I can't rely on calendar time for scheduling
// (which is all standard libc provides)
#define _POSIX_C_SOURCE 199309L

#include <errno.h>
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

static inline void ms_to_timespec(int64_t ms, struct timespec *ts)
{
	ts->tv_sec = ms / MS_PER_SEC;
	ts->tv_nsec = (ms % MS_PER_SEC) * NS_PER_MS;
}

uint64_t timestamp_ms_get(void)
{
	struct timespec ts;
	// CLOCK_BOOTTIME is preferred here because it is not affected by large jitters
	// due to NTP or settimeofday (unlike CLOCK_REALTIME) and includes suspend time
	// (unlike CLOCK_MONOTONIC).
	(void)clock_gettime(CLOCK_BOOTTIME, &ts);
	return timespec_to_ms(&ts);
}

void sleep_ms(uint64_t ms)
{
	struct timespec ts;
	struct timespec rem;
	ms_to_timespec(ms, &rem);
	int sleep_result = -1;
	do {
		ts = rem;
		sleep_result = nanosleep(&ts, &rem);
	} while (0 != sleep_result && EINTR == errno);
}

#undef NS_PER_MS
#undef MS_PER_SEC
#undef _POSIX_C_SOURCE
