/**
 * @file kmalloc.h
 * @brief Kernel Memory Allocator - Standard interface for dynamic memory
 *
 * Provides Linux-like kmalloc/kfree interface built on top of slab allocator.
 * Supports allocation flags, aligned allocations, and memory tracking.
 */

#ifndef KMALLOC_H
#define KMALLOC_H

#include <_cheader.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Allocation Flags (GFP flags)
// ============================================================================

/**
 * @brief Allocation flags (similar to Linux GFP flags)
 */
typedef enum
{
	KMALLOC_KERNEL = 0x00,	// Normal kernel allocation
	KMALLOC_ATOMIC = 0x01,	// Cannot sleep (interrupts/atomic context)
	KMALLOC_ZERO = 0x02,	// Zero-initialize memory
	KMALLOC_DMA = 0x04,	// DMA-capable memory (not implemented yet)
	KMALLOC_NOWAIT = 0x08,	// Don't wait if no memory available
	KMALLOC_NORETRY = 0x10, // Don't retry on failure
	KMALLOC_USER = 0x20,	// User-accessible memory (future)
} kmalloc_flags_t;

// Convenience macros (Linux-like)
#define GFP_KERNEL KMALLOC_KERNEL
#define GFP_ATOMIC KMALLOC_ATOMIC
#define GFP_ZERO (KMALLOC_KERNEL | KMALLOC_ZERO)
#define GFP_DMA KMALLOC_DMA

// ============================================================================
// Core Functions
// ============================================================================

_Begin_C_Header;

/**
 * @brief Allocate kernel memory
 *
 * @param size Size in bytes
 * @param flags Allocation flags
 * @return Pointer to allocated memory or NULL
 *
 * Examples:
 *   void *ptr = kmalloc(256, GFP_KERNEL);
 *   void *atomic_ptr = kmalloc(64, GFP_ATOMIC);
 *   void *zero_ptr = kmalloc(128, GFP_ZERO);
 */
void *kmalloc(size_t size, kmalloc_flags_t flags);

/**
 * @brief Free kernel memory
 *
 * @param ptr Pointer to memory (can be NULL)
 *
 * Example:
 *   kfree(ptr);
 */
void kfree(void *ptr);

/**
 * @brief Allocate zeroed kernel memory
 *
 * @param size Size in bytes
 * @return Pointer to zeroed memory or NULL
 *
 * Equivalent to: kmalloc(size, GFP_ZERO)
 */
void *kzalloc(size_t size);

/**
 * @brief Allocate array of elements
 *
 * @param n Number of elements
 * @param size Size of each element
 * @param flags Allocation flags
 * @return Pointer to array or NULL
 *
 * Example:
 *   int *array = kmalloc_array(10, sizeof(int), GFP_KERNEL);
 */
void *kmalloc_array(size_t n, size_t size, kmalloc_flags_t flags);

/**
 * @brief Allocate zeroed array
 *
 * @param n Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed array or NULL
 */
void *kcalloc(size_t n, size_t size);

/**
 * @brief Reallocate kernel memory
 *
 * @param ptr Old pointer (can be NULL)
 * @param new_size New size
 * @param flags Allocation flags
 * @return New pointer or NULL
 *
 * Example:
 *   ptr = krealloc(ptr, 512, GFP_KERNEL);
 */
void *krealloc(void *ptr, size_t new_size, kmalloc_flags_t flags);

/**
 * @brief Duplicate a memory region
 *
 * @param src Source memory
 * @param size Size to duplicate
 * @param flags Allocation flags
 * @return Pointer to duplicated memory or NULL
 */
void *kmemdup(const void *src, size_t size, kmalloc_flags_t flags);

/**
 * @brief Duplicate a string
 *
 * @param s String to duplicate
 * @param flags Allocation flags
 * @return Pointer to duplicated string or NULL
 */
char *kstrdup(const char *s, kmalloc_flags_t flags);

/**
 * @brief Duplicate a string with maximum length
 *
 * @param s String to duplicate
 * @param max Maximum length
 * @param flags Allocation flags
 * @return Pointer to duplicated string or NULL
 */
char *kstrndup(const char *s, size_t max, kmalloc_flags_t flags);

// ============================================================================
// Aligned Allocations
// ============================================================================

/**
 * @brief Allocate aligned memory
 *
 * @param size Size in bytes
 * @param align Alignment (must be power of 2)
 * @param flags Allocation flags
 * @return Aligned pointer or NULL
 *
 * Example:
 *   void *ptr = kmalloc_aligned(1024, 64, GFP_KERNEL); // 64-byte aligned
 */
void *kmalloc_aligned(size_t size, size_t align, kmalloc_flags_t flags);

/**
 * @brief Free aligned memory
 *
 * @param ptr Pointer allocated with kmalloc_aligned
 */
void kfree_aligned(void *ptr);

// ============================================================================
// Size Tracking
// ============================================================================

/**
 * @brief Get size of allocated object
 *
 * @param ptr Pointer to allocated memory
 * @return Size in bytes or 0 if not found
 *
 * Note: Returns the actual allocated size (may be larger than requested)
 */
size_t ksize(void *ptr);

// ============================================================================
// Statistics
// ============================================================================

/**
 * @brief Kernel memory statistics
 */
typedef struct
{
	uint64_t total_allocated;   // Total bytes ever allocated
	uint64_t total_freed;	    // Total bytes ever freed
	uint64_t current_allocated; // Currently allocated bytes
	uint64_t peak_allocated;    // Peak memory usage
	uint64_t alloc_count;	    // Number of allocations
	uint64_t free_count;	    // Number of frees
	uint64_t failed_allocs;	    // Failed allocations
} kmalloc_stats_t;

/**
 * @brief Get kmalloc statistics
 *
 * @return Pointer to statistics structure
 */
kmalloc_stats_t *kmalloc_get_stats(void);

/**
 * @brief Print kmalloc statistics
 */
void kmalloc_print_stats(void);

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Allocate typed object
 */
#define kmalloc_type(type, flags) \
	((type *)kmalloc(sizeof(type), flags))

/**
 * @brief Allocate zeroed typed object
 */
#define kzalloc_type(type) \
	((type *)kzalloc(sizeof(type)))

/**
 * @brief Allocate typed array
 */
#define kmalloc_array_type(type, count, flags) \
	((type *)kmalloc_array(count, sizeof(type), flags))

/**
 * @brief Allocate zeroed typed array
 */
#define kcalloc_type(type, count) \
	((type *)kcalloc(count, sizeof(type)))

// ============================================================================
// Size Classes (for documentation)
// ============================================================================

/**
 * Standard kmalloc size classes:
 *   8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 bytes
 *
 * For larger allocations, use:
 *   - vmalloc() for non-contiguous memory (future)
 *   - kmalloc_large() for multi-page allocations (future)
 */

// Maximum kmalloc size (same as SLAB_MAX_SIZE)
#define KMALLOC_MAX_SIZE 4096

// Minimum kmalloc size
#define KMALLOC_MIN_SIZE 8

_End_C_Header;

#endif /* KMALLOC_H */
