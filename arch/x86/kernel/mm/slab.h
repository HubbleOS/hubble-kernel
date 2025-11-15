/**
 * @file slab.h
 * @brief Slab Allocator - Efficient Kernel Memory Allocator
 *
 * Slab allocator для швидкого виділення об'єктів фіксованого розміру.
 * Підтримує різні розміри кешів та зменшує фрагментацію пам'яті.
 */

#ifndef SLAB_H
#define SLAB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Configuration
// ============================================================================

#define SLAB_MIN_SIZE 8	    // Мінімальний розмір об'єкта
#define SLAB_MAX_SIZE 4096  // Максимальний розмір об'єкта
#define SLAB_CACHE_COUNT 12 // Кількість стандартних кешів

// ============================================================================
// Slab Structures
// ============================================================================

/**
 * @brief Один slab (сторінка з об'єктами)
 */
typedef struct slab
{
	struct slab *next; // Наступний slab в списку
	struct slab *prev; // Попередній slab
	void *free_list;   // Список вільних об'єктів
	uint32_t in_use;   // Кількість використаних об'єктів
	uint32_t capacity; // Загальна кількість об'єктів
	void *start;	   // Початок даних
} slab_t;

/**
 * @brief Slab cache (кеш для об'єктів певного розміру)
 */
typedef struct slab_cache
{
	size_t object_size; // Розмір об'єкта
	size_t align;	    // Вирівнювання

	slab_t *slabs_full;    // Повністю зайняті slabs
	slab_t *slabs_partial; // Частково зайняті slabs
	slab_t *slabs_free;    // Вільні slabs

	uint32_t objects_per_slab; // Об'єктів на slab
	uint32_t total_slabs;	   // Загальна кількість slabs
	uint32_t total_objects;	   // Загальна кількість об'єктів
	uint32_t used_objects;	   // Використано об'єктів

	struct slab_cache *next; // Наступний кеш
} slab_cache_t;

/**
 * @brief Slab allocator statistics
 */
typedef struct
{
	uint64_t total_memory;	    // Загальна пам'ять під slab
	uint64_t used_memory;	    // Використана пам'ять
	uint64_t wasted_memory;	    // Витрачена пам'ять (fragmentation)
	uint32_t cache_count;	    // Кількість кешів
	uint32_t total_slabs;	    // Загальна кількість slabs
	uint32_t total_allocations; // Загальна кількість виділень
	uint32_t total_frees;	    // Загальна кількість звільнень
	uint32_t cache_hits;	    // Влучання в кеш
	uint32_t cache_misses;	    // Промахи кешу
} slab_info_t;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * @brief Initialize slab allocator
 *
 * Створює стандартні кеші для розмірів: 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
 */
void slab_init(void);

/**
 * @brief Allocate memory from slab
 *
 * @param size Size in bytes
 * @return Pointer to allocated memory or NULL
 *
 * Example:
 *   void *ptr = slab_alloc(64);
 */
void *slab_alloc(size_t size);

/**
 * @brief Free memory allocated by slab
 *
 * @param ptr Pointer to memory
 *
 * Example:
 *   slab_free(ptr);
 */
void slab_free(void *ptr);

/**
 * @brief Allocate zeroed memory
 *
 * @param size Size in bytes
 * @return Pointer to zeroed memory or NULL
 */
void *slab_calloc(size_t size);

/**
 * @brief Reallocate memory
 *
 * @param ptr Old pointer
 * @param new_size New size
 * @return New pointer or NULL
 */
void *slab_realloc(void *ptr, size_t new_size);

// ============================================================================
// Cache Management
// ============================================================================

/**
 * @brief Create custom cache
 *
 * @param size Object size
 * @param align Alignment (must be power of 2)
 * @return Pointer to cache or NULL
 *
 * Example:
 *   slab_cache_t *task_cache = slab_cache_create(512, 8);
 */
slab_cache_t *slab_cache_create(size_t size, size_t align);

/**
 * @brief Allocate from specific cache
 *
 * @param cache Cache to allocate from
 * @return Pointer to object or NULL
 */
void *slab_cache_alloc(slab_cache_t *cache);

/**
 * @brief Free to specific cache
 *
 * @param cache Cache to free to
 * @param ptr Pointer to object
 */
void slab_cache_free(slab_cache_t *cache, void *ptr);

/**
 * @brief Destroy cache
 *
 * @param cache Cache to destroy
 */
void slab_cache_destroy(slab_cache_t *cache);

/**
 * @brief Shrink cache (free empty slabs)
 *
 * @param cache Cache to shrink
 * @return Number of freed slabs
 */
uint32_t slab_cache_shrink(slab_cache_t *cache);

// ============================================================================
// Statistics and Debugging
// ============================================================================

/**
 * @brief Get slab allocator statistics
 *
 * @return Pointer to statistics structure
 */
slab_info_t *slab_get_info(void);

/**
 * @brief Print cache information
 *
 * @param cache Cache to print info about
 */
void slab_cache_info(slab_cache_t *cache);

/**
 * @brief Print all caches
 */
void slab_print_caches(void);

/**
 * @brief Validate slab consistency
 *
 * @return true if consistent, false otherwise
 */
bool slab_validate(void);

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Allocate typed object
 */
#define slab_alloc_type(type) \
	((type *)slab_alloc(sizeof(type)))

/**
 * @brief Allocate typed array
 */
#define slab_alloc_array(type, count) \
	((type *)slab_alloc(sizeof(type) * (count)))

/**
 * @brief Allocate zeroed typed object
 */
#define slab_calloc_type(type) \
	((type *)slab_calloc(sizeof(type)))

#endif /* SLAB_H */
