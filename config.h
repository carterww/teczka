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

// Event config
#define EVENT_IO_CURL_BUFFER_LEN (8)
// This param for epoll has been ignored since Linux 2.6.8 but we'll
// make a sensible default for portability
#define EVENT_LOOP_EPOLL_SIZE (8)
#define EVENT_LOOP_EPOLL_EVENTS_LEN (4)

#define EVENT_LOOP_FDS_MAX (16)

#endif // _TECZKA_CONFIG_H
