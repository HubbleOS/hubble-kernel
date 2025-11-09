#include "pmm.h"
#include "printk.h"
#include <string.h>

#define PAGE_SIZE 0x1000

static uint64_t heap_start_virt;
static uint64_t heap_start_phys;
static uint64_t heap_size;

static uint64_t total_pages;
static uint8_t *bitmap = NULL;
static size_t bitmap_size;

static inline void set_page(size_t page)
{
	bitmap[page / 8] |= 1 << (page % 8);
}

static inline void clear_page(size_t page)
{
	bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int test_page(size_t page)
{
	return bitmap[page / 8] & (1 << (page % 8));
}

void pmm_init(uint64_t pmm_start, uint64_t pmm_size)
{
	printk("=== PMM Init ===\n");
	printk("Physical region: 0x%llx - 0x%llx (%llu MB)\n",
	       pmm_start, pmm_start + pmm_size, pmm_size / (1024 * 1024));

	heap_start_phys = pmm_start;
	heap_start_virt = pmm_start;
	heap_size = pmm_size;

	total_pages = heap_size / PAGE_SIZE;
	bitmap = (uint8_t *)heap_start_virt;
	bitmap_size = (total_pages + 7) / 8;

	printk("Bitmap: %llu bytes (%llu KB)\n",
	       bitmap_size, bitmap_size / 1024);

	memset(bitmap, 0, bitmap_size);

	uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
	heap_start_virt += bitmap_pages * PAGE_SIZE;
	heap_start_phys += bitmap_pages * PAGE_SIZE;
	heap_size -= bitmap_pages * PAGE_SIZE;
	total_pages = heap_size / PAGE_SIZE;

	printk("Usable: %llu pages (%llu MB)\n",
	       total_pages, total_pages * PAGE_SIZE / (1024 * 1024));
	printk("=== PMM Init Complete ===\n");
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

				return (void *)(heap_start_virt + start_page * PAGE_SIZE);
			}
		}
		else
			consecutive = 0;
	}

	return NULL;
}

void pmm_free(void *addr, size_t pages)
{
	size_t page = ((uint64_t)addr - heap_start_virt) / PAGE_SIZE;
	if (page >= total_pages)
		return;

	for (size_t i = 0; i < pages; i++)
		clear_page(page + i);
}

uint64_t pmm_get_phys(void *virt_ptr)
{
	uint64_t offset = (uint64_t)virt_ptr - heap_start_virt;
	return heap_start_phys + offset;
}
