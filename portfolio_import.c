#include "equity.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "portfolio.h"
#include "portfolio_import.h"
#include "static_mem_cache.h"
#include "teczka_string.h"

// NOTE: If any members are not char *, please chnage code in _fill_values
struct _fidelity_line_strings {
	char *account_number;
	char *account_name;
	char *ticker;
	char *name;
	char *quantity;
	char *last_price;
	char *last_price_change_abs;
	char *current_value;
	char *todays_change_abs;
	char *todays_change_percent;
	char *lifetime_change_abs;
	char *lifetime_change_percent;
	char *percent_of_account;
	char *cost_basis_total;
	char *cost_basis_per_share;
	char *type;
};

enum _fidelity_equity_fill_error {
	_FIDELITY_EQUITY_FILL_ERROR_OK = 0,
	_FIDELITY_EQUITY_FILL_ERROR_INVALID_LINE,
	_FIDELITY_EQUITY_FILL_ERROR_IGNORED_TICKER,
	_FIDELITY_EQUITY_FILL_ERROR_EOF,
	_FIDELITY_EQUITY_FILL_ERROR_TICKER_TOO_LONG,
	_FIDELITY_EQUITY_FILL_ERROR_NAME_TOO_LONG,
};

static enum _fidelity_equity_fill_error
_fidelity_equity_fill(struct equity *equity, char *buf);

static int _ticker_ignored(const char *ticker);

enum portfolio_import_error
portfolio_import_fidelity(struct portfolio *portfolio,
			  struct static_mem_cache *equity_cache,
			  const char *fidelity_csv_path)
{
	if (NULL == portfolio || NULL == equity_cache ||
	    NULL == fidelity_csv_path) {
		return PORTFOLIO_IMPORT_ERROR_NULL_ARG;
	}

	FILE *fid_csv = fopen(fidelity_csv_path, "r");
	if (NULL == fid_csv) {
		if (EACCES == errno) {
			return PORTFOLIO_IMPORT_ERROR_EACCESS_ERR;
		} else {
			return PORTFOLIO_IMPORT_ERROR_OPEN_ERR;
		}
	}
	char fgets_line_buf[PORTFOLIO_IMPORT_BUFFER_BYTES];
	const char *fgets_result = NULL;
	int skipped_header = 0;
	while (1) {
		fgets_result = fgets(fgets_line_buf,
				     PORTFOLIO_IMPORT_BUFFER_BYTES, fid_csv);
		// NULL indicates an errro or EOF.
		if (NULL == fgets_result) {
			if (feof(fid_csv)) {
				break;
			} else {
				fclose(fid_csv);
				return PORTFOLIO_IMPORT_ERROR_READ_ERR;
			}
		}
		// Make sure we read the entire line. If not, return buffer too small err
		if (NULL == strchr(fgets_line_buf, '\n')) {
			fclose(fid_csv);
			return PORTFOLIO_IMPORT_ERROR_BUFFER_TOO_SMALL;
		}
		// Skip first line
		if (!skipped_header) {
			skipped_header = 1;
			continue;
		}
		// At this point we know we have a whole CSV line in the buffer
		struct static_mem_cache_malloc_result eqm_result =
			static_mem_cache_malloc(equity_cache);
		if (STATIC_MEM_CACHE_MALLOC_ERROR_OK != eqm_result.error ||
		    NULL == eqm_result.ptr) {
			fclose(fid_csv);
			return PORTFOLIO_IMPORT_ERROR_EQUITY_CACHE_OOM;
		}
		struct equity_node *equity_node =
			(struct equity_node *)eqm_result.ptr;
		enum _fidelity_equity_fill_error fill_result =
			_fidelity_equity_fill(&equity_node->equity,
					      fgets_line_buf);
		switch (fill_result) {
			enum portfolio_equity_add_error add_result;
		case _FIDELITY_EQUITY_FILL_ERROR_OK:
			// Don't need to check return value here. The only error is if
			// either arg is NULL but we've checked both
			add_result =
				portfolio_equity_add(portfolio, equity_node);
			// Free the node if it was merged into one
			if (PORTFOLIO_EQUITY_ADD_ERROR_MERGED == add_result) {
				(void)static_mem_cache_free(equity_cache,
							    equity_node);
			}
			break;
		case _FIDELITY_EQUITY_FILL_ERROR_INVALID_LINE:
			fclose(fid_csv);
			return PORTFOLIO_IMPORT_ERROR_INVALID_CSV;
		case _FIDELITY_EQUITY_FILL_ERROR_IGNORED_TICKER:
			// None of these errors should happen in this case.
			(void)static_mem_cache_free(equity_cache, equity_node);
			continue;
		case _FIDELITY_EQUITY_FILL_ERROR_EOF:
			// None of these errors for free should happen in this case.
			(void)static_mem_cache_free(equity_cache, equity_node);
			goto exit_loop;
		case _FIDELITY_EQUITY_FILL_ERROR_NAME_TOO_LONG:
		case _FIDELITY_EQUITY_FILL_ERROR_TICKER_TOO_LONG:
			// None of these errors for free should happen in this case.
			(void)static_mem_cache_free(equity_cache, equity_node);
			break;
		default:
			exit(1); // Shouldn't be possible
		}
	}
exit_loop:
	// Update portfolio value equities
	portfolio_update_values(portfolio);

	fclose(fid_csv);
	return PORTFOLIO_IMPORT_ERROR_OK;
}

// My special version of strtok. It does the regular strtok but also specifies
// a too_far param.  If the character strtok takes us to is in too_far, NULL is
// returned. Othersise, the pointer to the character after delim is returned.
static inline char *__strtok(char *buf, const char *delim, const char *too_far)
{
	char *strtok_token = strtok(buf, delim);
	if (NULL == strtok_token) {
		return NULL;
	}
	while ('\0' != *too_far) {
		if (*strtok_token == *too_far) {
			return NULL;
		}
		++too_far;
	}
	return strtok_token;
}

// Returns 0 on success, nonzero if the ticker is ignored.
static int _fill_values(struct _fidelity_line_strings *values, char *buf)
{
	static const char *delim = ",";
	static const char *too_far = "\r\n";
	values->account_number = __strtok(buf, delim, too_far);
	values->account_name = __strtok(NULL, delim, too_far);
	values->ticker = __strtok(NULL, delim, too_far);
	if (_ticker_ignored(values->ticker)) {
		return 1;
	}
	values->name = __strtok(NULL, delim, too_far);
	values->quantity = __strtok(NULL, delim, too_far);
	values->last_price = __strtok(NULL, delim, too_far);
	values->last_price_change_abs = __strtok(NULL, delim, too_far);
	values->current_value = __strtok(NULL, delim, too_far);
	values->todays_change_abs = __strtok(NULL, delim, too_far);
	values->todays_change_percent = __strtok(NULL, delim, too_far);
	values->lifetime_change_abs = __strtok(NULL, delim, too_far);
	values->lifetime_change_percent = __strtok(NULL, delim, too_far);
	values->percent_of_account = __strtok(NULL, delim, too_far);
	values->cost_basis_total = __strtok(NULL, delim, too_far);
	values->cost_basis_per_share = __strtok(NULL, delim, too_far);
	values->type = __strtok(NULL, delim, too_far);

	return 0;
}

// Returns true if the values are valid (not null) false otherwise.
static int _values_valid(struct _fidelity_line_strings *values)
{
	// Make sure none of the pointers are NULL.
	// This is very unsafe. Lord please forgive me.
	const size_t values_len = sizeof(*values) / sizeof(char *);
	char **values_arr = (char **)values;
	for (size_t i = 0; i < values_len; ++i) {
		if (NULL == values_arr[i]) {
			return 0;
		}
	}

	// Cut off the name if it's too long
	if (strnlen(values->name, EQUITY_NAME_BYTES_MAX + 1) >
	    EQUITY_NAME_BYTES_MAX) {
		values->name[EQUITY_NAME_BYTES_MAX] = '\0';
	}

	return 1;
}

static void _ownership_init(struct equity_ownership *ownership,
			    struct _fidelity_line_strings *values)
{
	(void)equity_ownership_zero(ownership);
	ownership->share_count_hundredths = string_to_int64_hundredths(
		values->quantity, strlen(values->quantity));
	ownership->cost_basis_cents = string_to_int64_hundredths(
		values->cost_basis_total, strlen(values->cost_basis_total));
}

static void _valuation_init(struct equity_valuation *valuation,
			    struct _fidelity_line_strings *values)
{
	int64_t price_cents_current = string_to_int64_hundredths(
		values->last_price, strlen(values->last_price));
	int64_t daily_change_absolute_cents = string_to_int64_hundredths(
		values->todays_change_abs, strlen(values->todays_change_abs));
	*valuation = (struct equity_valuation){
		.price_cents_current = price_cents_current,
		.price_cents_open = 0, // Don't have :(
		.price_cents_close_previous =
			price_cents_current - daily_change_absolute_cents,
		.daily_change_absolute_cents = daily_change_absolute_cents,
		.daily_change_basis_points = string_to_int64_hundredths(
			values->todays_change_percent,
			strlen(values->todays_change_percent)),
	};
}

static enum _fidelity_equity_fill_error
_fidelity_equity_fill(struct equity *equity, char *buf)
{
	// After all data rows, there are empty rows and rows with text that begin with a quotation.
	// We should tell the caller we have reached this point
	if ('\0' == buf[0] || '"' == buf[0] || '\n' == buf[0] ||
	    '\r' == buf[0]) {
		return _FIDELITY_EQUITY_FILL_ERROR_EOF;
	}
	struct _fidelity_line_strings values;
	int fill_result = _fill_values(&values, buf);
	if (0 != fill_result) {
		return _FIDELITY_EQUITY_FILL_ERROR_IGNORED_TICKER;
	}
	int values_valid = _values_valid(&values);
	if (!values_valid) {
		return _FIDELITY_EQUITY_FILL_ERROR_INVALID_LINE;
	}
	enum equity_id_error equity_init_id_result =
		equity_init_id(equity, values.ticker, values.name);
	switch (equity_init_id_result) {
	case EQUITY_ID_ERROR_OK:
		break;
	case EQUITY_ID_ERROR_ID_TOO_LONG:
		return _FIDELITY_EQUITY_FILL_ERROR_TICKER_TOO_LONG;
	case EQUITY_ID_ERROR_NAME_TOO_LONG:
		return _FIDELITY_EQUITY_FILL_ERROR_NAME_TOO_LONG;
	case EQUITY_ID_ERROR_NULL_EQUITY:
	default:
		exit(1); // Shouldn't be possible
	}
	_ownership_init(&equity->ownership, &values);
	_valuation_init(&equity->valuation, &values);

	// Update deltas with new values
	(void)equity_ownership_deltas_update(&equity->ownership,
					     &equity->valuation);
	return _FIDELITY_EQUITY_FILL_ERROR_OK;
}

static int _ticker_ignored(const char *ticker)
{
	size_t num_ticker =
		sizeof(PORTFOLIO_IMPORT_TICKER_IGNORE) / sizeof(char *);
	for (size_t i = 0; i < num_ticker; ++i) {
		const char *ignored_ticker = PORTFOLIO_IMPORT_TICKER_IGNORE[i];
		if (!strcmp(ignored_ticker, ticker)) {
			return 1;
		}
	}
	return 0;
}
