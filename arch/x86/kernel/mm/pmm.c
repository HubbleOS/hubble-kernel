#include "pmm.h"
#include <string.h>
#include <stdbool.h>
#include "higher_half.h"
#include "lib/bitmap.h"

#include "printk.h"

static pmm_info_t g_pmm_info = {0};
static uint64_t g_heap_phys_start = 0;
static uint64_t g_heap_phys_end = 0;
static uint64_t g_last_search_index = 0;

// Converting page index to physical address
static inline uint64_t page_index_to_phys(uint64_t index)
{
	return g_heap_phys_start + (index * PAGE_SIZE);
}

// Converting the page index to virtual (higher-half)
static inline uint64_t page_index_to_virt(uint64_t index)
{
	return PHYS_TO_VIRT(page_index_to_phys(index));
}

// Marking pages as busy/free
static void mark_pages(uint64_t start_index, size_t count, bool used)
{
	for (size_t i = 0; i < count; i++)
	{
		uint64_t idx = start_index + i;
		if (idx >= g_pmm_info.total_pages)
			break;

		if (used)
		{
			if (!bitmap_test(g_pmm_info.bitmap, idx))
			{
				bitmap_set(g_pmm_info.bitmap, idx);
				g_pmm_info.used_pages++;
				g_pmm_info.used_memory += PAGE_SIZE;
			}
		}
		else
		{
			if (bitmap_test(g_pmm_info.bitmap, idx))
			{
				bitmap_clear(g_pmm_info.bitmap, idx);
				g_pmm_info.used_pages--;
				g_pmm_info.used_memory -= PAGE_SIZE;
			}
		}
	}
	if (!used && start_index < g_last_search_index)
		g_last_search_index = start_index;
}

// ================= Allocation =================

static int find_consecutive_free_pages(uint64_t count, uint64_t *start_index)
{
	if (count == 0 || count > g_pmm_info.total_pages)
		return -1;

	uint64_t consecutive = 0;
	uint64_t first_index = 0;

	for (uint64_t i = g_last_search_index; i < g_pmm_info.total_pages + g_last_search_index; i++)
	{
		uint64_t idx = i % g_pmm_info.total_pages;
		if (!bitmap_test(g_pmm_info.bitmap, idx))
		{
			if (consecutive == 0)
				first_index = idx;
			consecutive++;
			if (consecutive == count)
			{
				*start_index = first_index;
				return 0;
			}
		}
		else
		{
			consecutive = 0;
		}
	}
	return -1;
}

uint64_t pmm_alloc_pages(size_t count)
{
	if (count == 0)
		return 0;

	uint64_t start_index;
	if (find_consecutive_free_pages(count, &start_index) != 0)
		return 0;

	mark_pages(start_index, count, true);
	g_last_search_index = (start_index + count) % g_pmm_info.total_pages;

	return page_index_to_phys(start_index);
}

uint64_t pmm_alloc_page(void)
{
	return pmm_alloc_pages(1);
}

// ================= Free =================

void pmm_free_pages(uint64_t virt_addr, size_t count)
{
	if (virt_addr == 0 || count == 0)
		return;

	if (virt_addr % PAGE_SIZE != 0)
		return;

	uint64_t phys_addr = VIRT_TO_PHYS(virt_addr);

	if (phys_addr < g_heap_phys_start || phys_addr >= g_heap_phys_end)
		return;

	uint64_t start_index = (phys_addr - g_heap_phys_start) / PAGE_SIZE;

	if (start_index + count > g_pmm_info.total_pages)
		count = g_pmm_info.total_pages - start_index;

	mark_pages(start_index, count, false);
}

void pmm_free_page(uint64_t virt_addr)
{
	pmm_free_pages(virt_addr, 1);
}

// ================= Initialization =================

void pmm_init(uint64_t heap_phys_start, uint64_t heap_size)
{
	// Aligning the start and heap size
	heap_phys_start = PAGE_ALIGN_UP(heap_phys_start);
	heap_size = PAGE_ALIGN_DOWN(heap_size);

	uint64_t total_pages = heap_size / PAGE_SIZE;

	// Calculate the bitmap size in bytes
	uint64_t bitmap_size_bytes = (total_pages + 7) / 8;

	// Align the bitmap size to the page size
	uint64_t bitmap_size = PAGE_ALIGN_UP(bitmap_size_bytes);

	// Calculate the number of pages for a bitmap
	uint64_t bitmap_pages = bitmap_size / PAGE_SIZE;

	// Place the bitmap at the beginning of the heap
	g_pmm_info.bitmap = (uint8_t *)PHYS_TO_VIRT(heap_phys_start);
	g_pmm_info.bitmap_size = bitmap_size;

	// Clearing bitmap
	memset(g_pmm_info.bitmap, 0, bitmap_size);

	// Configure heap parameters (subtract bitmap)
	g_heap_phys_start = heap_phys_start + (bitmap_pages * PAGE_SIZE);
	g_pmm_info.total_pages = total_pages - bitmap_pages;
	g_pmm_info.total_memory = g_pmm_info.total_pages * PAGE_SIZE;
	g_heap_phys_end = g_heap_phys_start + g_pmm_info.total_memory;

	g_pmm_info.used_pages = 0;
	g_pmm_info.used_memory = 0;
	g_last_search_index = 0;
}
