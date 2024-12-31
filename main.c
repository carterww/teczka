#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>
#include <curl/multi.h>

#include "config.h"
#include "event.h"
#include "portfolio.h"
#include "portfolio_import.h"
#include "static_mem_cache.h"

// Static buffers that will be passed to static mem caches to control
static struct equity_node EQUITY_NODE_STATIC_BUFFER[MEM_CACHE_EQUITY_NODE_COUNT];
static struct event_node EVENT_NODE_STATIC_BUFFER[MEM_CACHE_EVENT_NODE_COUNT];

static struct static_mem_cache equity_node_cache;
static struct static_mem_cache event_node_cache;
static struct portfolio portfolio;
static struct event_queue event_queue;

static CURLM *curl_multi_handle = NULL;

static char *fidelity_csv_path_get(int argc, char *argv[]);
static int globals_init(void);
static void globals_cleanup(void);

void _curl_init(void);
void _curl_cleanup(void);

int main(int argc, char *argv[])
{
	const char *fidelity_csv_path = fidelity_csv_path_get(argc, argv);
	if (NULL == fidelity_csv_path) {
		printf("No file path provided in CLI arguments.\n");
		return 1;
	}
	int init_globals_result = globals_init();
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

	globals_cleanup();
	return 0;
}

static char *fidelity_csv_path_get(int argc, char *argv[])
{
	if (argc < 2) {
		return NULL;
	}
	return argv[1];
}

static int globals_init(void)
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

	const int event_node_cache_init_res = static_mem_cache_init(
		&event_node_cache, EVENT_NODE_STATIC_BUFFER,
		MEM_CACHE_EVENT_NODE_COUNT, sizeof(struct event_node),
		STATIC_MEM_CACHE_FLAG_CHECK_FREE_LIST_ON_FREE);
	if (STATIC_MEM_CACHE_INIT_ERROR_OK != event_node_cache_init_res) {
		printf("Failed to initialize the event static mem cache with result %d\n",
		       event_node_cache_init_res);
		return 1;
	}

	const int portfolio_init_res = portfolio_init(&portfolio);
	if (portfolio_init_res) {
		printf("Failed to initialize the portfolio\n");
		return 1;
	}

	const int event_queue_init_res = event_queue_init(&event_queue);
	if (event_queue_init_res) {
		printf("Failed to initialize the event queue\n");
		return 1;
	}
	_curl_init();
	return 0;
}

static void globals_cleanup(void)
{
	_curl_cleanup();
}

void _curl_init(void)
{
	CURLcode curl_init_result =
		curl_global_init(CURL_GLOBAL_SSL | CURL_GLOBAL_ACK_EINTR);
	if (CURLE_OK != curl_init_result) {
		printf("curl_global_init failed with code %d\n",
		       curl_init_result);
		exit(1);
	}
	curl_multi_handle = curl_multi_init();
	if (NULL == curl_multi_handle) {
		printf("curl_multi_init failed\n");
		exit(1);
	}
}
void _curl_cleanup(void)
{
	CURLMcode multi_cleanup_result = curl_multi_cleanup(curl_multi_handle);
	if (CURLM_OK != multi_cleanup_result) {
		printf("curl_multi_cleanup failed with code %d\n",
		       multi_cleanup_result);
		curl_global_cleanup();
		exit(1);
	}
	curl_global_cleanup();
}
