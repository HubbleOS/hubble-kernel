/**
 * @file slab.h
 * @brief Slab Allocator - Efficient kernel memory allocator for fixed-size
 *        objects
 *
 * Provides caches for different object sizes to reduce fragmentation and
 * improve allocation performance.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <_cheader.h>

/* ── Configuration ───────────────────────────────────────────────────────── */

#define SLAB_MIN_SIZE 8
#define SLAB_MAX_SIZE 1024

/* ── Slab Structures ─────────────────────────────────────────────────────── */

/**
 * @brief A single slab (one page of objects)
 */
typedef struct slab {
	struct slab *next;
	struct slab *prev;
	void *free_list;
	uint32_t in_use;
	uint32_t capacity;
	void *start;
} slab_t;

/**
 * @brief Slab cache for objects of a specific size
 */
typedef struct slab_cache {
	size_t object_size;
	size_t align;

	slab_t *slabs_full;
	slab_t *slabs_partial;
	slab_t *slabs_free;

	uint32_t objects_per_slab;
	uint32_t total_slabs;
	uint32_t total_objects;
	uint32_t used_objects;

	struct slab_cache *next;
} slab_cache_t;

/**
 * @brief Slab allocator statistics
 */
typedef struct {
	uint64_t total_memory;
	uint64_t used_memory;
	uint64_t wasted_memory;
	uint32_t cache_count;
	uint32_t total_slabs;
	uint32_t total_allocations;
	uint32_t total_frees;
	uint32_t cache_hits;
	uint32_t cache_misses;
} slab_info_t;

_Begin_C_Header;

/* ── Core Functions ──────────────────────────────────────────────────────── */

/**
 * @brief Initialize the slab allocator
 *
 * Creates standard caches for sizes: 8, 16, 32, 64, 128, 256, 512, 1024
 */
void slab_init(void);

/**
 * @brief Allocate memory from the slab allocator
 *
 * @param size Size in bytes
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *slab_alloc(size_t size);

/**
 * @brief Free memory allocated by the slab allocator
 *
 * @param ptr Pointer to the memory to free
 */
void slab_free(void *ptr);

/**
 * @brief Allocate zero-initialized memory
 *
 * @param size Size in bytes
 * @return Pointer to zeroed memory, or NULL on failure
 */
void *slab_calloc(size_t size);

/**
 * @brief Reallocate memory from the slab allocator
 *
 * @param ptr Old pointer
 * @param new_size New size in bytes
 * @return New pointer, or NULL on failure
 */
void *slab_realloc(void *ptr, size_t new_size);

/* ── Cache Management ────────────────────────────────────────────────────── */

/**
 * @brief Find the slab cache that owns a given pointer
 *
 * @param ptr Pointer to look up
 * @return Cache containing the pointer, or NULL
 */
slab_cache_t *find_cache_for_ptr(void *ptr);

/**
 * @brief Create a custom slab cache
 *
 * @param size Object size
 * @param align Alignment (must be a power of 2)
 * @return Pointer to the new cache, or NULL on failure
 */
slab_cache_t *slab_cache_create(size_t size, size_t align);

/**
 * @brief Allocate an object from a specific cache
 *
 * @param cache Cache to allocate from
 * @return Pointer to the allocated object, or NULL on failure
 */
void *slab_cache_alloc(slab_cache_t *cache);

/**
 * @brief Free an object back to its specific cache
 *
 * @param cache Cache to free to
 * @param ptr Pointer to the object
 */
void slab_cache_free(slab_cache_t *cache, void *ptr);

/**
 * @brief Allocate a page for slab use
 *
 * @return Virtual address of the page, or NULL on failure
 */
void *slab_alloc_page(void);

/**
 * @brief Free a page allocated by slab
 *
 * @param addr Virtual address of the page
 */
void slab_free_page(void *addr);

_End_C_Header;
