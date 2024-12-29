#ifndef _TECZKA_EQUITY_H
#define _TECZKA_EQUITY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "config.h"
#include "teczka_string.h"

/* equity_ownership is a struct that stores the data regarding
 * the user's ownership of an equity.
 */
struct equity_ownership {
	int64_t share_count_hundredths;
	int64_t cost_basis_cents;
	int64_t delta_lifetime_absolute_cents;
	int64_t delta_lifetime_basis_points;
	int64_t delta_daily_absolute_cents;
	int64_t delta_daily_basis_points;
};

/* equity_valuation is a struct that stores the data regarding the
 * equity's current valuation.
 */
struct equity_valuation {
	int64_t price_cents_current;
	int64_t price_cents_open;
	int64_t price_cents_close_previous;
	int64_t daily_change_absolute_cents;
	int64_t daily_change_basis_points;
};

/* equity is a struct that combines the valuation and ownership with identifying
 * information to give a wholistic view of the equity. equity will be stored in the
 * portfolio.
 */
struct equity {
	char key[EQUITY_KEY_BYTES_MAX + 1];
	struct equity_valuation valuation;
	struct equity_ownership ownership;
	char name[EQUITY_NAME_BYTES_MAX + 1];
};

// Error enum returned by equity_init_id.
enum equity_id_error {
	EQUITY_ID_ERROR_OK = 0,
	EQUITY_ID_ERROR_NULL_EQUITY,
	EQUITY_ID_ERROR_ID_TOO_LONG,
	EQUITY_ID_ERROR_NAME_TOO_LONG,
};

/* Takes the share price of an equity (cents) and the share count (hundredths) and
 * returns the total value of the position in cents.
 */
static inline int64_t equity_total_value_cents(int64_t share_value_cents,
					       int64_t share_count_hundredths)
{
	return (share_value_cents * share_count_hundredths) / 100;
}
/* Takes the total value of a holding (cents) and the share count (hundredths) and
 * returns the per share price of the equity in cents.
 */
static inline int64_t
equity_per_share_value_cents(int64_t value_total_cents,
			     int64_t share_count_hundredths)
{
	return value_total_cents / share_count_hundredths;
}

/* Takes an absolute delta in cents and an original value in cents and returns the
 * delta in basis points.
 */
static inline int64_t delta_basis_points(int64_t delta_abs_cents,
					 int64_t original_value_abs_cents)
{
	// We cannot divide by 0 so we will return the closest thing to infinity
	// we have!
	if (original_value_abs_cents == 0) {
		return INT64_MAX;
	}
	// Multiply the absolute delta by 100 twice. Once to get percent then another
	// to get basis points.
	return (delta_abs_cents * 100 * 100) / original_value_abs_cents;
}

/* Note about these functions:
 * If the function returns an int, a return value of 0 = success and
 * other = failure.
 * If the failure case isn't documented, it is most likely because an argument
 * was a NULL pointer. 
 * If there is more than one failure case, it will return an enum. 0 should be the
 * success case in the enum but I wouldn't rely on that.
 */
static inline int equity_ownership_zero(struct equity_ownership *equity)
{
	if (NULL == equity) {
		return 1;
	}
	memset(equity, 0, sizeof(*equity));
	return 0;
}

static inline int equity_ownership_copy(struct equity_ownership *equity,
					const struct equity_ownership *other)
{
	if (NULL == equity || NULL == other) {
		return 1;
	}
	memcpy(equity, other, sizeof(*equity));
	return 0;
}

static inline int
equity_ownership_merge(struct equity_ownership *add_to,
		       const struct equity_ownership *add_from)
{
	if (NULL == add_to || NULL == add_from) {
		return 1;
	}
	add_to->cost_basis_cents += add_from->cost_basis_cents;
	add_to->share_count_hundredths += add_from->share_count_hundredths;
	return 0;
}

static inline int equity_valuation_zero(struct equity_valuation *equity)
{
	if (NULL == equity) {
		return 1;
	}
	memset(equity, 0, sizeof(*equity));
	return 0;
}

static inline int equity_valuation_copy(struct equity_valuation *equity,
					const struct equity_valuation *other)
{
	if (NULL == equity || NULL == other) {
		return 1;
	}
	memcpy(equity, other, sizeof(*equity));
	return 0;
}

/* Updates all the deltas in ownership based on the equity's current valuation.
 * @param ownership: Nonnull pointer to an equity_ownership struct.
 * @param valuation: Nonnull pointer to an equity_valuation struct.
 * @returns 0 on success, something else if ownership or valuation is NULL.
 */
static inline int
equity_ownership_deltas_update(struct equity_ownership *ownership,
			       const struct equity_valuation *valuation)
{
	if (NULL == ownership || NULL == valuation) {
		return 1;
	}

	const int64_t current_value_total =
		equity_total_value_cents(valuation->price_cents_current,
					 ownership->share_count_hundredths);
	ownership->delta_lifetime_absolute_cents =
		current_value_total - ownership->cost_basis_cents;
	ownership->delta_lifetime_basis_points =
		delta_basis_points(ownership->delta_lifetime_absolute_cents,
				   ownership->cost_basis_cents);

	const int64_t daily_delta_absolute_per_share =
		valuation->price_cents_current -
		valuation->price_cents_close_previous;
	ownership->delta_daily_absolute_cents =
		equity_total_value_cents(daily_delta_absolute_per_share,
					 ownership->share_count_hundredths);
	ownership->delta_daily_absolute_cents =
		delta_basis_points(ownership->delta_daily_absolute_cents,
				   ownership->cost_basis_cents);
	return 0;
}

static inline int equity_zero(struct equity *equity)
{
	if (NULL == equity) {
		return 1;
	}
	memset(equity, 0, sizeof(*equity));
	return 0;
}

static inline int equity_copy(struct equity *equity, const struct equity *other)
{
	if (NULL == equity || NULL == other) {
		return 1;
	}
	memcpy(equity, other, sizeof(*equity));
	return 0;
}

static inline int equity_merge(struct equity *add_to,
			       const struct equity *add_from)
{
	if (NULL == add_to || NULL == add_from) {
		return 1;
	}
	// These will no return non zero because we already ensured pointers
	// are not NULL
	(void)equity_ownership_merge(&add_to->ownership, &add_from->ownership);
	(void)equity_ownership_deltas_update(&add_to->ownership,
					     &add_to->valuation);
	return 0;
}

/* Initializes the key and name of the equity. Please ensure key and name are
 * less than the max bytes defined in config.h. If they are not, an error will
 * be returned.
 * @param equity: Nonnull pointer to an equity.
 * @param key: Pointer to a key that should be <= EQUITY_KEY_BYTES_MAX. If it
 * is not, equity_id_error.EQUITY_ID_ERROR_ID_TOO_LONG will be returned. If key
 * is NULL, equity's key is not altered.
 * @param name: Pointer to a name that should be <= EQUITY_NAME_BYTES_MAX. If
 * it is not, equity_id_error.EQUITY_ID_ERROR_NAME_TOO_LONG will be returned. If
 * name is NULL, equity's name will not be altered.
 */
static inline enum equity_id_error
equity_init_id(struct equity *equity, const char *key, const char *name)
{
	if (NULL == equity) {
		return EQUITY_ID_ERROR_NULL_EQUITY;
	}

	if (NULL != key) {
		size_t key_len = strnlen(key, EQUITY_KEY_BYTES_MAX + 1);
		if (key_len > EQUITY_KEY_BYTES_MAX) {
			return EQUITY_ID_ERROR_ID_TOO_LONG;
		}
		(void)strcpy(equity->key, key);
	}
	if (NULL != name) {
		size_t name_len = strnlen(name, EQUITY_NAME_BYTES_MAX + 1);
		if (name_len > EQUITY_NAME_BYTES_MAX) {
			return EQUITY_ID_ERROR_NAME_TOO_LONG;
		}
		(void)strcpy(equity->name, name);
	}

	return EQUITY_ID_ERROR_OK;
}

#endif // _TECZKA_EQUITY_H
