/**
 * @file kmalloc.h
 * @brief Kernel Memory Allocator - Standard interface for dynamic memory
 *
 * Provides a Linux-like kmalloc/kfree interface built on top of the slab
 * allocator. Supports allocation flags, aligned allocations, and memory
 * tracking.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <_cheader.h>

/* -- Allocation Flags ------------------------------------------------------ */

/**
 * @brief Allocation flags (similar to Linux GFP flags)
 */
typedef enum {
  KMALLOC_KERNEL = 0x00,
  KMALLOC_ATOMIC = 0x01,
  KMALLOC_ZERO = 0x02,
  KMALLOC_DMA = 0x04,
  KMALLOC_NOWAIT = 0x08,
  KMALLOC_NORETRY = 0x10,
  KMALLOC_USER = 0x20,
} kmalloc_flags_t;

#define GFP_KERNEL KMALLOC_KERNEL
#define GFP_ATOMIC KMALLOC_ATOMIC
#define GFP_ZERO (KMALLOC_KERNEL | KMALLOC_ZERO)
#define GFP_DMA KMALLOC_DMA

_Begin_C_Header;

/* -- Core Functions -------------------------------------------------------- */

/**
 * @brief Allocate kernel memory
 *
 * @param size Size in bytes
 * @param flags Allocation flags (GFP_* / KMALLOC_*)
 * @return Pointer to the allocated memory, or NULL on failure
 */
void *kmalloc(size_t size, kmalloc_flags_t flags);

/**
 * @brief Free kernel memory
 *
 * @param ptr Pointer to the memory to free (may be NULL)
 */
void kfree(void *ptr);

/**
 * @brief Allocate zero-initialized kernel memory
 *
 * Equivalent to kmalloc(size, GFP_ZERO).
 *
 * @param size Size in bytes
 * @return Pointer to zeroed memory, or NULL on failure
 */
void *kzalloc(size_t size);

/**
 * @brief Allocate an array of elements
 *
 * @param n Number of elements
 * @param size Size of each element
 * @param flags Allocation flags
 * @return Pointer to the array, or NULL on failure
 */
void *kmalloc_array(size_t n, size_t size, kmalloc_flags_t flags);

/**
 * @brief Allocate a zero-initialized array
 *
 * Equivalent to kmalloc_array(n, size, GFP_ZERO).
 *
 * @param n Number of elements
 * @param size Size of each element
 * @return Pointer to zeroed array, or NULL on failure
 */
void *kcalloc(size_t n, size_t size);

/**
 * @brief Reallocate kernel memory
 *
 * @param ptr Old pointer (may be NULL)
 * @param new_size New size in bytes
 * @param flags Allocation flags
 * @return New pointer, or NULL on failure
 */
void *krealloc(void *ptr, size_t new_size, kmalloc_flags_t flags);

/**
 * @brief Get the allocated size of a memory object
 *
 * @param ptr Pointer to the allocated memory
 * @return Size in bytes, or 0 if not found
 */
size_t ksize(void *ptr);

/* -- Utility Functions ----------------------------------------------------- */

/**
 * @brief Duplicate a memory region
 *
 * @param src Source memory
 * @param size Number of bytes to duplicate
 * @param flags Allocation flags
 * @return Pointer to the duplicate, or NULL on failure
 */
void *kmemdup(const void *src, size_t size, kmalloc_flags_t flags);

/**
 * @brief Duplicate a string
 *
 * @param s String to duplicate
 * @param flags Allocation flags
 * @return Pointer to the duplicated string, or NULL on failure
 */
char *kstrdup(const char *s, kmalloc_flags_t flags);

/**
 * @brief Duplicate a string with a maximum length
 *
 * @param s String to duplicate
 * @param max Maximum number of characters
 * @param flags Allocation flags
 * @return Pointer to the duplicated string, or NULL on failure
 */
char *kstrndup(const char *s, size_t max, kmalloc_flags_t flags);

_End_C_Header;
