#ifndef _TECZKA_PORTFOLIO_H
#define _TECZKA_PORTFOLIO_H

#include <stddef.h>
#include <stdint.h>

#include "equity.h"
#include "kette.h"

struct equity_node {
	struct dlink link;
	struct equity equity;
};

struct portfolio {
	struct dlink equity_head;
	int64_t market_value_cents;
	int64_t cost_basis_cents;
	int64_t delta_lifetime_absolute_cents;
	int64_t delta_lifetime_basis_points;
	int64_t delta_daily_absolute_cents;
	int64_t delta_daily_basis_points;
};

enum portfolio_equity_get_error {
	PORTFOLIO_EQUITY_GET_ERROR_OK = 0,
	PORTFOLIO_EQUITY_GET_ERROR_NULL_KEY,
	PORTFOLIO_EQUITY_GET_ERROR_NULL_PORTFOLIO,
	PORTFOLIO_EQUITY_GET_ERROR_KEY_TOO_LONG,
	PORTFOLIO_EQUITY_GET_ERROR_EQUITY_DNE,
};
struct portfolio_equity_get_result {
	enum portfolio_equity_get_error error;
	struct equity_node *equity;
};

enum portfolio_equity_add_error {
	PORTFOLIO_EQUITY_ADD_ERROR_OK = 0,
	PORTFOLIO_EQUITY_ADD_ERROR_NULL_ARG,
	PORTFOLIO_EQUITY_ADD_ERROR_MERGED,
};

enum portfolio_equity_remove_error {
	PORTFOLIO_EQUITY_REMOVE_ERROR_OK = 0,
	PORTFOLIO_EQUITY_REMOVE_ERROR_INVALID_EQUITY_ARG,
	PORTFOLIO_EQUITY_REMOVE_ERROR_NULL_PORTFOLIO,
	PORTFOLIO_EQUITY_REMOVE_ERROR_EQUITY_DNE,
};

/* These functions follow the same convention as the ones defined in equity.h. To
 * see more, read the comment above the function definitions there.
 * Tldr if a function returns an int, 0 = no error and nonzero = error. These functions
 * only have one error case. If there is more than one error case, an enum will be
 * returned.
 */

// Zeros out the market value, cost basis, and deltas.
static inline int portfolio_zero_values(struct portfolio *portfolio)
{
	if (NULL == portfolio) {
		return 1;
	}
	portfolio->market_value_cents = 0;
	portfolio->cost_basis_cents = 0;
	portfolio->delta_lifetime_absolute_cents = 0;
	portfolio->delta_lifetime_basis_points = 0;
	portfolio->delta_daily_absolute_cents = 0;
	portfolio->delta_daily_basis_points = 0;
	return 0;
}

// Initializes the portfolio
static inline int portfolio_init(struct portfolio *portfolio)
{
	if (NULL == portfolio) {
		return 1;
	}
	dlist_init(&portfolio->equity_head);
	portfolio_zero_values(portfolio);
	return 0;
}

/* Updates the market value, cost basis and deltas for the portfolio based
 * on the values in the equities.
 */
int portfolio_update_values(struct portfolio *portfolio);

struct portfolio_equity_get_result
portfolio_equity_get(struct portfolio *portfolio, const char *key);

struct portfolio_equity_get_result
portfolio_equity_get_next(struct portfolio *portfolio, const char *key);

enum portfolio_equity_add_error
portfolio_equity_add(struct portfolio *portfolio, struct equity_node *equity);

enum portfolio_equity_remove_error
portfolio_equity_remove(struct portfolio *portfolio,
			struct equity_node *equity);

enum portfolio_equity_remove_error
portfolio_equity_remove_by_key(struct portfolio *portfolio, const char *key);

#endif // _TECZKA_PORTFOLIO_H
