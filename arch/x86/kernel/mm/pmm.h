/**
 * @file pmm.h
 * @brief Physical Memory Manager - Page-level physical memory allocation
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* -- Page Constants -------------------------------------------------------- */

#define PAGE_SIZE 4096

#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

/* -- PMM State ------------------------------------------------------------- */

/**
 * @brief Physical Memory Manager state information
 */
typedef struct {
  uint64_t total_memory;
  uint64_t usable_memory;
  uint64_t used_memory;
  uint64_t total_pages;
  uint64_t used_pages;
  uint8_t *bitmap;
  uint64_t bitmap_size;
  uint32_t *page_refcounts;
} pmm_info_t;

/* -- Public API ------------------------------------------------------------ */

/**
 * @brief Initialize the Physical Memory Manager
 */
void pmm_init(void);

/**
 * @brief Allocate a single physical page (4 KB)
 *
 * @return Physical address of the page, or 0 on failure
 */
uint64_t pmm_alloc_page(void);

/**
 * @brief Allocate multiple consecutive physical pages
 *
 * @param count Number of pages to allocate
 * @return Physical address of the first page, or 0 on failure
 */
uint64_t pmm_alloc_pages(size_t count);

/**
 * @brief Free a single physical page
 *
 * @param phys_addr Physical address of the page to free
 */
void pmm_free_page(uint64_t phys_addr);

/**
 * @brief Free multiple consecutive physical pages
 *
 * @param phys_addr Physical address of the first page
 * @param count Number of pages to free
 */
void pmm_free_pages(uint64_t phys_addr, size_t count);
