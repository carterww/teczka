#ifndef _TECZKA_STRING_H
#define _TECZKA_STRING_H

#include <stddef.h>
#include <stdint.h>

// I'm using C11 standard and -Wpedantic so I have to define this myself. I don't
// want to use strlen
static inline size_t strnlen(const char *str, size_t max_len)
{
	if (NULL == str) {
		return 0;
	}
	size_t i;
	for (i = 0; i < max_len && str[i] != '\0'; ++i)
		;
	return i;
}

/* Converts a number string to an int64 in hundredths. The number string can
 * be a decimal.
 * If there are more digits past the hundredths place, the function truncates them.
 * This means it rounds the hundredths place down.
 * Ex) "1.629" is returned as 162
 * The string can have symbols and other non-numerical characters. These are simply ignored
 * (except for "-" and ".").
 * Ex) "$1.96" is returned as 196
 * Ex) "-$1.96" is returned as -196
 */
static inline int64_t string_to_int64_hundredths(const char *num, size_t len)
{
	int64_t scaler = 100;
	size_t up_to_hundredths_index = len - 1;

	int reached_decimal = 0;
	for (size_t i = 0; i < len && num[i] != '\0'; ++i) {
		if (num[i] == '.') {
			reached_decimal = 1;
			continue;
		}
		if (reached_decimal && (num[i] <= '9' && num[i] >= '0')) {
			switch (scaler) {
			case 100:
				up_to_hundredths_index = i;
				scaler = 10;
				break;
			case 10:
				up_to_hundredths_index = i;
				scaler = 1;
				break;
			case 1:
				break;
			default:
				break;
			}
		}
	}
	int64_t int_hundredths = 0;
	// Add one here so i doesn't underflow to max size_t when i = 0
	size_t i = up_to_hundredths_index + 1;
	while (i > 0) {
		const char digit = num[i - 1];
		if (digit <= '9' && digit >= '0') {
			int_hundredths += (digit - '0') * scaler;
			scaler = scaler * 10;
		} else if (digit == '-') {
			int_hundredths = int_hundredths * -1;
			break;
		}
		i = i - 1;
	}
	return int_hundredths;
}

#endif // _TECZKA_STRING_H
