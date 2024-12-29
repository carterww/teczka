#ifndef _TECZKA_PORTFOLIO_IMPORT_H
#define _TECZKA_PORTFOLIO_IMPORT_H

#include "portfolio.h"
#include "static_mem_cache.h"

enum portfolio_import_error {
	PORTFOLIO_IMPORT_ERROR_OK = 0,
	PORTFOLIO_IMPORT_ERROR_NULL_ARG,
	PORTFOLIO_IMPORT_ERROR_EACCESS_ERR,
	PORTFOLIO_IMPORT_ERROR_EQUITY_CACHE_OOM,
	PORTFOLIO_IMPORT_ERROR_INVALID_CSV,
	PORTFOLIO_IMPORT_ERROR_OPEN_ERR,
	PORTFOLIO_IMPORT_ERROR_READ_ERR,
	PORTFOLIO_IMPORT_ERROR_BUFFER_TOO_SMALL,
};

enum portfolio_import_error
portfolio_import_fidelity(struct portfolio *portfolio,
			  struct static_mem_cache *equity_cache,
			  const char *fidelity_csv_path);

#endif // _TECZKA_PORTFOLIO_IMPORT_H
