#ifndef _TECZKA_CONFIG_H
#define _TECZKA_CONFIG_H

// Sizes of static memory allocation config
#define MEM_CACHE_EQUITY_NODE_COUNT (64)
#define MEM_CACHE_EVENT_NODE_COUNT (6)

// Equity/Portfolio config
#define EQUITY_KEY_BYTES_MAX (7)
#define EQUITY_NAME_BYTES_MAX (31)

// Import config
#define PORTFOLIO_IMPORT_BUFFER_BYTES (1024)
static const char *PORTFOLIO_IMPORT_TICKER_IGNORE[] = {
	"SPAXX**",
	"Pending Activity",
};

#endif // _TECZKA_CONFIG_H
