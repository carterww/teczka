#include <stddef.h>

#include "static_mem_cache.h"

// This function intializes the free list for buffer and returns the pointer
// that was not added to the free list.
static void *_buffer_init_free_list(void *buffer, size_t buffer_size_bytes,
				    size_t buffer_element_size_bytes);

// Returns true if the contents of cache appear to be valid. This just means
// buffer is not NULL, the size members are not 0, etc.
static int _static_mem_cache_valid(const struct static_mem_cache *cache);

// Returns true if the ptr is in the cache's free list.
static int _ptr_in_free_list(const struct static_mem_cache *cache,
			     const void *ptr);

enum static_mem_cache_init_error
static_mem_cache_init(struct static_mem_cache *cache, void *buffer,
		      size_t buffer_elements_count,
		      size_t buffer_element_size_bytes, size_t flags)
{
	if (NULL == cache) {
		return STATIC_MEM_CACHE_INIT_ERROR_NULL_CACHE;
	}
	if (NULL == buffer || 0 == buffer_elements_count) {
		return STATIC_MEM_CACHE_INIT_ERROR_NO_BUFFER;
	}
	if (buffer_element_size_bytes < sizeof(void *)) {
		return STATIC_MEM_CACHE_INIT_ERROR_ELEMENT_TOO_SMALL;
	}
	cache->buffer = buffer;
	cache->buffer_size_bytes =
		buffer_elements_count * buffer_element_size_bytes;
	cache->buffer_element_size_bytes = buffer_element_size_bytes;
	cache->flags = flags;

	void *const unadded_free_ptr = _buffer_init_free_list(
		buffer, cache->buffer_size_bytes, buffer_element_size_bytes);
	cache->first_free = unadded_free_ptr;
	return STATIC_MEM_CACHE_INIT_ERROR_OK;
}

struct static_mem_cache_malloc_result
static_mem_cache_malloc(struct static_mem_cache *cache)
{
	struct static_mem_cache_malloc_result result = { 0 };
	if (NULL == cache) {
		result.error = STATIC_MEM_CACHE_MALLOC_ERROR_NULL_CACHE;
		return result;
	}
	if (!_static_mem_cache_valid(cache)) {
		result.error = STATIC_MEM_CACHE_MALLOC_ERROR_CORRUPTED_CACHE;
		return result;
	}
	if (NULL == cache->first_free) {
		result.error = STATIC_MEM_CACHE_MALLOC_ERROR_OOM;
		return result;
	}
	result.error = STATIC_MEM_CACHE_MALLOC_ERROR_OK;
	result.ptr = cache->first_free;
	cache->first_free = *(void **)result.ptr;

	return result;
}

enum static_mem_cache_free_error
static_mem_cache_free(struct static_mem_cache *cache, void *ptr)
{
	if (NULL == cache) {
		return STATIC_MEM_CACHE_FREE_ERROR_NULL_CACHE;
	}
	if (NULL == ptr) {
		return STATIC_MEM_CACHE_FREE_ERROR_OK;
	}
	if (!_static_mem_cache_valid(cache)) {
		return STATIC_MEM_CACHE_FREE_ERROR_CORRUPTED_CACHE;
	}
	const void *const buffer_end_exclusive =
		(void *)((size_t)cache->buffer + cache->buffer_size_bytes);
	if (ptr < cache->buffer && ptr >= buffer_end_exclusive) {
		return STATIC_MEM_CACHE_FREE_ERROR_NOT_IN_BUFFER;
	}
	if (cache->flags & STATIC_MEM_CACHE_FLAG_CHECK_FREE_LIST_ON_FREE &&
	    _ptr_in_free_list(cache, ptr)) {
		return STATIC_MEM_CACHE_FREE_ERROR_IN_FREE_LIST;
	}

	*(void **)ptr = cache->first_free;
	cache->first_free = ptr;
	return STATIC_MEM_CACHE_FREE_ERROR_OK;
}

static void *_buffer_init_free_list(void *buffer, size_t buffer_size_bytes,
				    size_t buffer_element_size_bytes)
{
	void *free_prev = NULL;
	void *buffer_end_exclusive =
		(void *)((size_t)buffer + buffer_size_bytes);
	// Here we loop through the buffer and set the value of each element
	// to the pointer of the previous element in the free list.
	while (buffer < buffer_end_exclusive) {
		*(void **)buffer = free_prev;
		free_prev = buffer;
		buffer = (void *)((size_t)buffer + buffer_element_size_bytes);
	}
	return free_prev;
}

static int _static_mem_cache_valid(const struct static_mem_cache *cache)
{
	return NULL != cache->buffer && cache->buffer_size_bytes > 0 &&
	       cache->buffer_element_size_bytes > 0;
}

static int _ptr_in_free_list(const struct static_mem_cache *cache,
			     const void *ptr)
{
	const void *first_free = cache->first_free;
	while (NULL != first_free) {
		if (first_free == ptr) {
			return 1;
		}
		first_free = *(void **)first_free;
	}
	return 0;
}
