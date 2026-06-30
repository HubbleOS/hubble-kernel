/**
 * @file vm_map.h
 * @brief Virtual Memory Area (VMA) map manager
 *
 * Manages a linked list of virtual memory areas for a process address space.
 * Provides insertion, lookup, and free-range search.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <smp/spinlock.h>

/* ── VMA Types ───────────────────────────────────────────────────────────── */

/**
 * @brief Type of a virtual memory area
 */
typedef enum {
	VMA_ANONYMOUS,
	VMA_FILE,
	VMA_DEVICE,
} vma_type_t;

/* ── VMA Flags ───────────────────────────────────────────────────────────── */

#define VM_READ  (1 << 0)
#define VM_WRITE (1 << 1)
#define VM_EXEC  (1 << 2)

/* ── Default Mapping Base ────────────────────────────────────────────────── */

#define MMAP_BASE 0x0000700000000000ULL

/* ── VMA and VM Map Structures ───────────────────────────────────────────── */

/**
 * @brief A single virtual memory area
 */
typedef struct vm_area {
	uint64_t base;
	size_t size;
	uint32_t flags;

	vma_type_t type;
	uint64_t phys_base;

	struct vm_area *next;
	struct vm_area *prev;
} vm_area_t;

/**
 * @brief A virtual memory map (address space)
 */
typedef struct {
	vm_area_t *areas;
	uint64_t mmap_base;
	spinlock_t lock;
} vm_map_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief Create a new virtual memory map
 *
 * @return Pointer to the new map, or NULL on failure
 */
vm_map_t *vm_map_create(void);

/**
 * @brief Insert a VMA into the map, sorted by base address
 *
 * @param map Target map
 * @param vma VMA to insert
 */
void vm_insert_area(vm_map_t *map, vm_area_t *vma);

/**
 * @brief Remove a VMA from the map
 *
 * @param map Target map
 * @param vma VMA to remove
 */
void vm_remove_area(vm_map_t *map, vm_area_t *vma);

/**
 * @brief Find the VMA containing a given address
 *
 * @param map Map to search
 * @param addr Virtual address
 * @return VMA containing the address, or NULL
 */
vm_area_t *vm_find_area(vm_map_t *map, uint64_t addr);

/**
 * @brief Find a free address range of the given size
 *
 * @param map Map to search
 * @param size Required size in bytes
 * @return Base address of the free range
 */
uint64_t vm_find_free_range(vm_map_t *map, size_t size);
