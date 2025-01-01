#ifndef _TECZKA_UTIL_H
#define _TECZKA_UTIL_H

#include <stddef.h>
#include <stdint.h>

struct data_buffer {
	char *buffer;
	size_t buffer_size_bytes;
	size_t buffer_used_bytes;
};

// NOT TIME SINCE EPOCH. DON'T EXPECT THAT
uint64_t timestamp_ms_get(void);

void sleep_ms(uint64_t ms);

#endif // _TECZKA_UTIL_H
