/**
 * @file kmalloc.h
 * @brief Unified kernel memory allocation interface (slab + VMM)
 */

#ifndef KMALLOC_H
#define KMALLOC_H

#include <_cheader.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <mm/slab.h>
#include <mm/vmm.h>

_Begin_C_Header;

/**
 * @brief Allocate kernel memory
 *
 * Uses slab for ≤ SLAB_MAX_SIZE and VMM for larger allocations.
 */
void *kmalloc(size_t size);

/**
 * @brief Free kernel memory
 *
 * Frees memory previously allocated by kmalloc/krealloc/kcalloc.
 */
void kfree(void *ptr);

/**
 * @brief Allocate zeroed kernel memory
 */
void *kcalloc(size_t size);

/**
 * @brief Reallocate kernel memory
 */
void *krealloc(void *ptr, size_t new_size);

_End_C_Header;

#endif /* KMALLOC_H */
