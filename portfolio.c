#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "equity.h"
#include "kette.h"
#include "portfolio.h"
#include "teczka_string.h"

static inline enum portfolio_equity_get_error
_portfolio_get_check_params(const struct portfolio *portfolio, const char *key);

int portfolio_update_values(struct portfolio *portfolio)
{
	if (NULL == portfolio) {
		return 1;
	}
	if (list_empty(&portfolio->equity_head)) {
		portfolio_zero_values(portfolio);
		return 0;
	}
	portfolio->market_value_cents = 0;
	portfolio->cost_basis_cents = 0;
	portfolio->delta_daily_absolute_cents = 0;
	struct equity_node *curr;
	list_for_each(&portfolio->equity_head, curr, struct equity_node, link) {
		portfolio->market_value_cents += equity_total_value_cents(
			curr->equity.valuation.price_cents_current,
			curr->equity.ownership.share_count_hundredths);
		portfolio->cost_basis_cents +=
			curr->equity.ownership.cost_basis_cents;
		portfolio->delta_daily_absolute_cents +=
			curr->equity.ownership.delta_daily_absolute_cents;
	}

	// We don't update absolute lifetime delta in the loop because it is more
	// efficient to do it in one operation. Sadly, we cannot do that with the absolute
	// daily delta.
	portfolio->delta_lifetime_absolute_cents =
		portfolio->market_value_cents - portfolio->cost_basis_cents;
	portfolio->delta_lifetime_basis_points =
		delta_basis_points(portfolio->delta_lifetime_absolute_cents,
				   portfolio->cost_basis_cents);

	// Unlike the absolute lifetime delta, we updated absolute daily delta in loop
	portfolio->delta_daily_basis_points =
		delta_basis_points(portfolio->delta_daily_absolute_cents,
				   portfolio->cost_basis_cents);

	return 0;
}

struct portfolio_equity_get_result
portfolio_equity_get(struct portfolio *portfolio, const char *key)
{
	struct portfolio_equity_get_result result = { 0 };
	result.error = _portfolio_get_check_params(portfolio, key);
	if (PORTFOLIO_EQUITY_GET_ERROR_OK != result.error) {
		return result;
	}
	// portfolio and key are nonnull here and strlen(key) <= EQUITY_KEY_BYTES_MAX
	struct equity_node *curr;
	list_for_each(&portfolio->equity_head, curr, struct equity_node, link) {
		const int key_diff = strcmp(key, curr->equity.key);
		if (0 == key_diff) {
			result.error = PORTFOLIO_EQUITY_GET_ERROR_OK;
			result.equity = curr;
			return result;
		}
	}
	result.error = PORTFOLIO_EQUITY_GET_ERROR_EQUITY_DNE;
	return result;
}

struct portfolio_equity_get_result
portfolio_equity_get_next(struct portfolio *portfolio, const char *key)
{
	struct portfolio_equity_get_result result = { 0 };
	result.error = _portfolio_get_check_params(portfolio, key);
	if (PORTFOLIO_EQUITY_GET_ERROR_OK != result.error) {
		return result;
	}
	// portfolio and key are nonnull here and strlen(key) <= EQUITY_KEY_BYTES_MAX
	struct equity_node *curr;
	list_for_each(&portfolio->equity_head, curr, struct equity_node, link) {
		const int key_diff = strcmp(key, curr->equity.key);
		// We go until we find an equity key > the passed in key. This should be
		// the next equity in the list. Since this is sorted, we'll never
		// return an equity with a key <= the passed in key.
		if (key_diff < 0) {
			result.error = PORTFOLIO_EQUITY_GET_ERROR_OK;
			result.equity = curr;
			return result;
		}
	}
	result.error = PORTFOLIO_EQUITY_GET_ERROR_EQUITY_DNE;
	return result;
}

enum portfolio_equity_add_error
portfolio_equity_add(struct portfolio *portfolio, struct equity_node *equity)
{
	if (NULL == portfolio || NULL == equity) {
		return PORTFOLIO_EQUITY_ADD_ERROR_NULL_ARG;
	}
	struct equity_node *curr;
	// The list is sorted on the key. We loop until we find an equity with a key >= the
	// equity to add's key. If we don't find one, curr will be the head of the list.
	list_for_each(&portfolio->equity_head, curr, struct equity_node, link) {
		const int key_diff =
			strcmp(equity->equity.key, curr->equity.key);
		if (key_diff < 0) {
			break;
		} else if (0 == key_diff) {
			// Combine the equities
			equity_merge(&curr->equity, &equity->equity);
			return PORTFOLIO_EQUITY_ADD_ERROR_MERGED;
		}
	}
	struct dlink *curr_link = &curr->link;
	// An edge case here is where curr is the equity_head of portfolio. This is bad
	// because curr is not really an equity_node.
	if (curr ==
	    list_entry(&portfolio->equity_head, struct equity_node, link)) {
		curr_link = &portfolio->equity_head;
	}
	// We want to add before the one we stopped on
	dlist_add(&equity->link, curr_link->prev);
	return PORTFOLIO_EQUITY_ADD_ERROR_OK;
}

enum portfolio_equity_remove_error
portfolio_equity_remove(struct portfolio *portfolio, struct equity_node *equity)
{
	if (NULL == portfolio) {
		return PORTFOLIO_EQUITY_REMOVE_ERROR_NULL_PORTFOLIO;
	}
	if (NULL == equity) {
		return PORTFOLIO_EQUITY_REMOVE_ERROR_INVALID_EQUITY_ARG;
	}
	dlist_del(&equity->link);
	return PORTFOLIO_EQUITY_REMOVE_ERROR_OK;
}

enum portfolio_equity_remove_error
portfolio_equity_remove_by_key(struct portfolio *portfolio, const char *key)
{
	struct portfolio_equity_get_result get_result =
		portfolio_equity_get(portfolio, key);
	// Make sure we return the corresponding remove error on get error.
	// If there is no error, we break from the switch.
	switch (get_result.error) {
	case PORTFOLIO_EQUITY_GET_ERROR_OK:
		break;
	case PORTFOLIO_EQUITY_GET_ERROR_KEY_TOO_LONG:
	case PORTFOLIO_EQUITY_GET_ERROR_NULL_KEY:
		return PORTFOLIO_EQUITY_REMOVE_ERROR_INVALID_EQUITY_ARG;
	case PORTFOLIO_EQUITY_GET_ERROR_NULL_PORTFOLIO:
		return PORTFOLIO_EQUITY_REMOVE_ERROR_NULL_PORTFOLIO;
	case PORTFOLIO_EQUITY_GET_ERROR_EQUITY_DNE:
		return PORTFOLIO_EQUITY_REMOVE_ERROR_EQUITY_DNE;
	default:
		// This shouldn't be possible
		exit(1);
	}
	return portfolio_equity_remove(portfolio, get_result.equity);
}

static inline enum portfolio_equity_get_error
_portfolio_get_check_params(const struct portfolio *portfolio, const char *key)
{
	if (NULL == portfolio) {
		return PORTFOLIO_EQUITY_GET_ERROR_NULL_PORTFOLIO;
	}
	if (NULL == key) {
		return PORTFOLIO_EQUITY_GET_ERROR_NULL_KEY;
	}
	// This isn't strictly necessary but it can
	// 1. Help catch logic errors
	// 2. Prevent cases where key does not have null terminating character
	const size_t key_len = strnlen(key, EQUITY_KEY_BYTES_MAX + 1);
	if (key_len > EQUITY_KEY_BYTES_MAX) {
		return PORTFOLIO_EQUITY_GET_ERROR_KEY_TOO_LONG;
	}

	return PORTFOLIO_EQUITY_GET_ERROR_OK;
}
