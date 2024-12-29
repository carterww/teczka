#ifndef _TECZKA_STATIC_MEM_CACHE_H
#define _TECZKA_STATIC_MEM_CACHE_H

#include <stddef.h>

/* static_mem_cache is a struct that stores the necessary information to
 * allocate and free from static buffer.
 * THIS IS NOT A GENERAL PURPOSE ALLOCATOR! This allocator should only be
 * used to manage an array of a single type.
 * Ex) If you have two arrays, say struct event_node x[64] and struct equity_node y[64],
 * you should use two separate static_mem_cache structs to keep track of each.
 */
struct static_mem_cache {
	void *first_free;
	void *buffer;
	size_t buffer_size_bytes;
	size_t buffer_element_size_bytes;
	size_t flags;
};

enum static_mem_cache_flags {
	STATIC_MEM_CACHE_FLAG_CHECK_FREE_LIST_ON_FREE = (1 << 0),
};

enum static_mem_cache_init_error {
	STATIC_MEM_CACHE_INIT_ERROR_OK = 0,
	STATIC_MEM_CACHE_INIT_ERROR_NULL_CACHE,
	STATIC_MEM_CACHE_INIT_ERROR_NO_BUFFER,
	STATIC_MEM_CACHE_INIT_ERROR_ELEMENT_TOO_SMALL,
};

enum static_mem_cache_malloc_error {
	STATIC_MEM_CACHE_MALLOC_ERROR_OK = 0,
	STATIC_MEM_CACHE_MALLOC_ERROR_NULL_CACHE,
	STATIC_MEM_CACHE_MALLOC_ERROR_CORRUPTED_CACHE,
	STATIC_MEM_CACHE_MALLOC_ERROR_OOM,
};
struct static_mem_cache_malloc_result {
	enum static_mem_cache_malloc_error error;
	void *ptr;
};

enum static_mem_cache_free_error {
	STATIC_MEM_CACHE_FREE_ERROR_OK = 0,
	STATIC_MEM_CACHE_FREE_ERROR_NULL_CACHE,
	STATIC_MEM_CACHE_FREE_ERROR_CORRUPTED_CACHE,
	STATIC_MEM_CACHE_FREE_ERROR_NOT_IN_BUFFER,
	STATIC_MEM_CACHE_FREE_ERROR_IN_FREE_LIST,
};

/* Initialize a static_mem_cache struct. By calling this function, you are passing
 * ownership of buffer to the static_mem_cache. Do not touch it after this.
 * @param cache: Nonnull pointer to a static_mem_cache struct. If cache is NULL,
 * static_mem_cache_init_error.STATIC_MEM_CACHE_INIT_ERROR_NULL_CACHE is returned.
 * @param buffer: Nonnull pointer to the buffer of elements. This should be an array of
 * some type that no other component should touch after calling this function.
 * @param buffer_elements_count: The length of the array at buffer.
 * @param buffer_element_size_bytes: The size (in bytes) of each individual element in
 * the array buffer. This number must be >= sizeof(void *). An embedded free list is used
 * to minimize overhead.
 * @returns A static_mem_cache_init_error enum. STATIC_MEM_CACHE_INIT_ERROR_OK indicates no error.
 * @error STATIC_MEM_CACHE_INIT_ERROR_NULL_CACHE: cache arg is NULL.
 * @error STATIC_MEM_CACHE_INIT_ERROR_NULL_BUFFER: buffer arg is NULL.
 * @error STATIC_MEM_CACHE_INIT_ERROR_ELEMENT_TOO_SMALL: The size of the element is too small to
 * contain a pointer. Since we use an embedded free list, this memory allocator will not
 * work with this type.
 */
enum static_mem_cache_init_error
static_mem_cache_init(struct static_mem_cache *cache, void *buffer,
		      size_t buffer_elements_count,
		      size_t buffer_element_size_bytes, size_t flags);

/* Allocate memory from the buffer controlled by the static_mem_cache cache.
 * @param cache: Nonnull pointer to the initialized static_mem_cache.
 * @returns A static_mem_cache_malloc_result struct. This struct contains an
 * enum that possibly indicates a error and a possibly NULL pointer to the allocated
 * memory. STATIC_MEM_CACHE_MALLOC_ERROR_OK as the error member indicates a success.
 * If STATIC_MEM_CACHE_MALLOC_ERROR_OK is not the error member, do not trust the value
 * in ptr (it's probably NULL though).
 * @error STATIC_MEM_CACHE_MALLOC_ERROR_NULL_CACHE: The arg cache was NULL.
 * @error STATIC_MEM_CACHE_MALLOC_ERROR_CORRUPTED_CACHE: The cache has a NULL buffer.
 * This could mean it wasn't initialized or something else.
 * @error STATIC_MEM_CACHE_MALLOC_ERROR_OOM: There is no free memory in buffer.
 */
struct static_mem_cache_malloc_result
static_mem_cache_malloc(struct static_mem_cache *cache);

/* Frees an element previously allocated by the static_mem_cache cache.
 * @param cache: Nonnull pointer to the initialized static_mem_cache.
 * @param ptr: Pointer to the memory to free. If ptr is NULL, the function just
 * returns ok.
 * @returns a static_mem_cache_free_error enum. If STATIC_MEM_CACHE_FREE_ERROR_OK is returned,
 * the function executed with no errors.
 * @error STATIC_MEM_CACHE_FREE_ERROR_NULL_CACHE: The arg cache was NULL.
 * @error STATIC_MEM_CACHE_FREE_ERROR_CORRUPTED_CACHE: The cache has a NULL buffer.
 * This could mean it wasn't initialized or something else.
 * @error STATIC_MEM_CACHE_FREE_ERROR_NOT_IN_BUFFER: The pointer ptr was not within the
 * bounds of the buffer.
 * @error STATIC_MEM_CACHE_FREE_ERROR_IN_FREE_LIST: The pointer ptr is in the buffer
 * but is already in the free list. This error is only returned if the flag 
 * STATIC_MEM_CACHE_FLAG_CHECK_FREE_LIST_ON_FREE is enabled for cache. This is recommended
 * because it can help catch bugs, but it makes this function's time complexity O(n) instead
 * of O(1).
 */
enum static_mem_cache_free_error
static_mem_cache_free(struct static_mem_cache *cache, void *ptr);

#endif // _TECZKA_STATIC_MEM_CACHE_H
