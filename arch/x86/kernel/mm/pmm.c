/**
 * @file pmm.c
 * @brief Physical Memory Manager implementation
 *
 * Manages physical memory pages using a bitmap allocator.
 * Tracks allocated and free pages in the physical address space.
 *
 * The PMM obtains its memory layout directly from the Limine
 * memory map response — no intermediate boot-info structure.
 */

#include <stdbool.h>

#include <boot/limine.h>
#include <requests.h>
#include <hubble/string.h>

#include "higher_half.h"
#include "lib/bitmap.h"

#include "pmm.h"

/* -- Global State ---------------------------------------------------------- */

static pmm_info_t g_pmm_info = {0};

/**
 * Physical address range managed by the PMM.
 * Set by start_kernel() from the Limine memory map before pmm_init().
 */
uint64_t g_pmm_heap_phys_start = 0;
uint64_t g_pmm_heap_phys_end = 0;

static uint64_t g_last_search_index = 0;

/* -- Internal Helpers ------------------------------------------------------ */

static inline uint64_t page_index_to_phys(uint64_t index) {
  return g_pmm_heap_phys_start + (index * PAGE_SIZE);
}

static void mark_pages(uint64_t start_index, size_t count, bool used) {
  for (size_t i = 0; i < count; i++) {
    uint64_t idx = start_index + i;
    if (idx >= g_pmm_info.total_pages)
      break;

    if (used) {
      if (!bitmap_test(g_pmm_info.bitmap, idx)) {
        bitmap_set(g_pmm_info.bitmap, idx);
        g_pmm_info.used_pages++;
        g_pmm_info.used_memory += PAGE_SIZE;
      }
    } else {
      if (bitmap_test(g_pmm_info.bitmap, idx)) {
        bitmap_clear(g_pmm_info.bitmap, idx);
        g_pmm_info.used_pages--;
        g_pmm_info.used_memory -= PAGE_SIZE;
      }
    }
  }
  if (!used && start_index < g_last_search_index)
    g_last_search_index = start_index;
}

/* -- Page Allocation ------------------------------------------------------- */

static int find_consecutive_free_pages(uint64_t count, uint64_t *start_index) {
  if (count == 0 || count > g_pmm_info.total_pages)
    return -1;

  uint64_t consecutive = 0;
  uint64_t first_index = 0;

  for (uint64_t i = g_last_search_index;
       i < g_pmm_info.total_pages + g_last_search_index; i++) {
    uint64_t idx = i % g_pmm_info.total_pages;
    if (!bitmap_test(g_pmm_info.bitmap, idx)) {
      if (consecutive == 0)
        first_index = idx;
      consecutive++;
      if (consecutive == count) {
        *start_index = first_index;
        return 0;
      }
    } else {
      consecutive = 0;
    }
  }
  return -1;
}

uint64_t pmm_alloc_pages(size_t count) {
  if (count == 0)
    return 0;

  uint64_t start_index;
  if (find_consecutive_free_pages(count, &start_index) != 0)
    return 0;

  mark_pages(start_index, count, true);
  g_last_search_index = (start_index + count) % g_pmm_info.total_pages;

  uint64_t phys_addr = page_index_to_phys(start_index);

  if (phys_addr & (PAGE_SIZE - 1)) {
    mark_pages(start_index, count, false);
    return 0;
  }
  return phys_addr;
}

uint64_t pmm_alloc_page(void) { return pmm_alloc_pages(1); }

/* -- Page Deallocation ----------------------------------------------------- */

void pmm_free_pages(uint64_t phys_addr, size_t count) {
  if (phys_addr == 0 || count == 0)
    return;

  if (phys_addr % PAGE_SIZE != 0)
    return;

  if (phys_addr < g_pmm_heap_phys_start || phys_addr >= g_pmm_heap_phys_end)
    return;

  uint64_t start_index = (phys_addr - g_pmm_heap_phys_start) / PAGE_SIZE;

  if (start_index + count > g_pmm_info.total_pages)
    count = g_pmm_info.total_pages - start_index;

  mark_pages(start_index, count, false);
}

void pmm_free_page(uint64_t phys_addr) { pmm_free_pages(phys_addr, 1); }

/* -- Initialization -------------------------------------------------------- */

/**
 * @brief Initialize the Physical Memory Manager
 *
 * Uses g_pmm_heap_phys_start / g_pmm_heap_phys_end which were set
 * by start_kernel() from the Limine memory map before this call.
 */
void pmm_init(void) {
  uint64_t heap_phys_start = PAGE_ALIGN_UP(g_pmm_heap_phys_start);
  uint64_t heap_phys_end = g_pmm_heap_phys_end;
  uint64_t heap_size = heap_phys_end - heap_phys_start;

  uint64_t total_pages = heap_size / PAGE_SIZE;
  uint64_t bitmap_size = PAGE_ALIGN_UP((total_pages + 7) / 8);

  /* Bitmap is placed at the start of the heap, accessed via HHDM */
  uint64_t bitmap_phys = heap_phys_start;
  uint64_t heap_after_bitmap = bitmap_phys + bitmap_size;
  g_pmm_heap_phys_start = PAGE_ALIGN_UP(heap_after_bitmap);

  uint64_t usable_size = heap_phys_end - g_pmm_heap_phys_start;

  g_pmm_info.bitmap = (uint8_t *)phys_to_virt(bitmap_phys);
  g_pmm_info.bitmap_size = bitmap_size;
  memset(g_pmm_info.bitmap, 0, bitmap_size);

  g_pmm_info.total_pages = usable_size / PAGE_SIZE;
  g_pmm_info.total_memory = g_pmm_info.total_pages * PAGE_SIZE;
  g_pmm_heap_phys_end = g_pmm_heap_phys_start + g_pmm_info.total_memory;

  g_pmm_info.used_pages = 0;
  g_pmm_info.used_memory = 0;
  g_last_search_index = 0;
}
