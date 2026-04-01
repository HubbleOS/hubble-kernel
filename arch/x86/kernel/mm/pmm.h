#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Default page size: 4KB
#define PAGE_SIZE 4096

// Macros for address alignment
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

// Framework for PMM information
typedef struct
{
	uint64_t total_memory;	// Total amount of memory in bytes
	uint64_t usable_memory; // Available memory
	uint64_t used_memory;	// Memory used
	uint64_t total_pages;	// Total number of pages
	uint64_t used_pages;	// Number of pages used
	uint8_t *bitmap;	// A pointer to a bitmap
	uint64_t bitmap_size;	// Bitmap size in bytes
} pmm_info_t;

/**
 * Initializes the Physical Memory Manager
 */
void pmm_init();

/**
 * Allocates one physical page (4KB)
 * @return The physical address of the selected page or 0 on error
 */
uint64_t pmm_alloc_page(void);

/**
 * Allocates multiple consecutive physical pages
 * @param count Number of pages
 * @return The physical address of the first page or 0 on error
 */
uint64_t pmm_alloc_pages(size_t count);

/**
 * Releases one physical page
 * @param addr The physical address of the page
 */
void pmm_free_page(uint64_t phys_addr);

/**
 * Frees multiple consecutive physical pages
 * @param addr The physical address of the first page
 * @param count Number of pages
 */
void pmm_free_pages(uint64_t phys_addr, size_t count);
