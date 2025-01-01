#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <curl/multi.h>

#include "config.h"
#include "event_loop.h"
#include "portfolio.h"
#include "portfolio_import.h"
#include "static_mem_cache.h"

// Static buffers that will be passed to static mem caches to control
static struct equity_node EQUITY_NODE_STATIC_BUFFER[MEM_CACHE_EQUITY_NODE_COUNT];

static struct static_mem_cache equity_node_cache;
static struct portfolio portfolio;

static char *_fidelity_csv_path_get(int argc, char *argv[]);
static int _portfolio_init(void);

int main(int argc, char *argv[])
{
	const char *fidelity_csv_path = _fidelity_csv_path_get(argc, argv);
	if (NULL == fidelity_csv_path) {
		printf("No file path provided in CLI arguments.\n");
		return 1;
	}
	int init_globals_result = _portfolio_init();
	if (init_globals_result) {
		return 1;
	}
	enum portfolio_import_error portfolio_import_res =
		portfolio_import_fidelity(&portfolio, &equity_node_cache,
					  fidelity_csv_path);
	if (PORTFOLIO_IMPORT_ERROR_OK != portfolio_import_res) {
		printf("Failed to import the portfolio with result %d\n",
		       portfolio_import_res);
		return 1;
	}
	enum event_loop_init_error event_loop_init_result = event_loop_init();
	if (EVENT_LOOP_INIT_ERROR_OK != event_loop_init_result) {
		printf("Failed to initialize the event loop with result %d\n",
		       event_loop_init_result);
		return 1;
	}
	return 0;
}

static char *_fidelity_csv_path_get(int argc, char *argv[])
{
	if (argc < 2) {
		return NULL;
	}
	return argv[1];
}

static int _portfolio_init(void)
{
	const int equity_node_cache_init_res = static_mem_cache_init(
		&equity_node_cache, EQUITY_NODE_STATIC_BUFFER,
		MEM_CACHE_EQUITY_NODE_COUNT, sizeof(struct equity_node),
		STATIC_MEM_CACHE_FLAG_CHECK_FREE_LIST_ON_FREE);
	if (STATIC_MEM_CACHE_INIT_ERROR_OK != equity_node_cache_init_res) {
		printf("Failed to initialize the equity static mem cache with result %d\n",
		       equity_node_cache_init_res);
		return 1;
	}

	const int portfolio_init_res = portfolio_init(&portfolio);
	if (portfolio_init_res) {
		printf("Failed to initialize the portfolio\n");
		return 1;
	}

	return 0;
}
