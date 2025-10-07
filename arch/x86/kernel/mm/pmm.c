#include <mm/pmm.h>
#include <lib/bitmap.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define PAGE_SIZE 0x1000 // 4 KB

static uint64_t heap_start;
static uint64_t heap_size;

static uint64_t total_pages;
static uint8_t *bitmap = NULL;
static size_t bitmap_size;

// Bitmap operations
static inline void set_page(size_t page) { bitmap[page / 8] |= 1 << (page % 8); }
static inline void clear_page(size_t page) { bitmap[page / 8] &= ~(1 << (page % 8)); }
static inline int test_page(size_t page) { return bitmap[page / 8] & (1 << (page % 8)); }

void pmm_init(uint64_t pmm_start, uint64_t pmm_size)
{
	heap_start = pmm_start;
	heap_size = pmm_size;

	total_pages = heap_size / PAGE_SIZE;
	bitmap = (uint8_t *)heap_start;
	bitmap_size = (total_pages + 7) / 8;

	memset(bitmap, 0, bitmap_size);

	// Move heap_start past the bitmap
	heap_start += bitmap_size;
	heap_size -= bitmap_size;
	total_pages = heap_size / PAGE_SIZE;
}

void *pmm_alloc(size_t pages)
{
	size_t consecutive = 0;
	size_t start_page = 0;

	for (size_t i = 0; i < total_pages; i++)
	{
		if (!test_page(i))
		{
			if (consecutive == 0)
				start_page = i;
			consecutive++;
			if (consecutive == pages)
			{
				for (size_t j = start_page; j < start_page + pages; j++)
					set_page(j);
				return (void *)(heap_start + start_page * PAGE_SIZE);
			}
		}
		else
			consecutive = 0;
	}

	return NULL; // No space
}

void pmm_free(void *addr, size_t pages)
{
	size_t page = ((uint64_t)addr - heap_start) / PAGE_SIZE;
	if (page >= total_pages)
		return;

	for (size_t i = 0; i < pages; i++)
		clear_page(page + i);
}
