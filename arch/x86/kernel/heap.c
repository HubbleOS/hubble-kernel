#include <heap.h>

#define PAGE_SIZE 0x1000 // 4KB

static uint64_t heap_start_addr = 0;
static uint64_t heap_end_addr = 0;
static size_t total_pages = 0;
static uint8_t *page_bitmap = NULL; // 1 bit per page

// Bitmap
static inline void set_page(size_t page) { page_bitmap[page / 8] |= 1 << (page % 8); }
static inline void clear_page(size_t page) { page_bitmap[page / 8] &= ~(1 << (page % 8)); }
static inline int test_page(size_t page) { return page_bitmap[page / 8] & (1 << (page % 8)); }

// Initialize the heap
void heap_init(uint64_t heap_start, uint64_t heap_size)
{
	heap_start_addr = heap_start;
	heap_end_addr = heap_start + heap_size;

	total_pages = heap_size / PAGE_SIZE;

	// Bitmap in start of heap
	page_bitmap = (uint8_t *)heap_start_addr;
	size_t bitmap_size = (total_pages + 7) / 8;
	for (size_t i = 0; i < bitmap_size; i++)
		page_bitmap[i] = 0; // all bits are 0

	// Move heap_start_addr to bitmap
	heap_start_addr += bitmap_size;
}

void *alloc_page()
{
	for (size_t i = 0; i < total_pages; i++)
	{
		if (!test_page(i))
		{
			set_page(i);
			return (void *)(heap_start_addr + i * PAGE_SIZE);
		}
	}
	return NULL; // No free pages
}

void *alloc_pages(size_t num_pages)
{
	size_t consecutive = 0;
	for (size_t i = 0; i < total_pages; i++)
	{
		if (!test_page(i))
		{
			consecutive++;
			if (consecutive == num_pages)
			{
				size_t start_page = i + 1 - num_pages;
				for (size_t j = start_page; j <= i; j++)
					set_page(j);
				return (void *)(heap_start_addr + start_page * PAGE_SIZE);
			}
		}
		else
		{
			consecutive = 0;
		}
	}
	return NULL; // No free pages
}

void free_page(void *ptr)
{
	size_t page = ((uint64_t)ptr - heap_start_addr) / PAGE_SIZE;
	clear_page(page);
}

void free_pages(void *ptr, size_t num_pages)
{
	size_t start_page = ((uint64_t)ptr - heap_start_addr) / PAGE_SIZE;
	for (size_t i = 0; i < num_pages; i++)
		clear_page(start_page + i);
}

void *kmalloc(size_t size)
{
	size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	if (pages_needed == 1)
		return alloc_page();
	else
		return alloc_pages(pages_needed);
}

void kfree(void *ptr)
{
	free_page(ptr);
}

memory_ops_t heap_memory_ops = {
    .malloc = kmalloc,
    .free = kfree,
};

// static uint64_t heap_ptr = 0;
// static uint64_t heap_end = 0;

// void heap_init(uint64_t heap_start, uint64_t heap_size)
// {
// 	heap_ptr = heap_start;
// 	heap_end = heap_start + heap_size;
// 	volatile uint8_t *ptr = (volatile uint8_t *)heap_start;
// 	for (size_t i = 0; i < heap_size; i++)
// 	{
// 		ptr[i] = 0xAA;
// 	}
// 	return;
// }

// void *kmalloc(size_t size)
// {
// 	if (heap_ptr + size > heap_end)
// 		return NULL;
// 	void *ptr = (void *)heap_ptr;
// 	heap_ptr += size;
// 	return ptr;
// }

// void *kmalloc_aligned(size_t size, size_t align)
// {
// 	uint64_t aligned_ptr = (heap_ptr + align - 1) & ~(align - 1);
// 	if (aligned_ptr + size > heap_end)
// 		return NULL;
// 	void *ptr = (void *)aligned_ptr;
// 	heap_ptr = aligned_ptr + size;
// 	return ptr;
// }

// void kfree(void *ptr) { (void)ptr; }

// memory_ops_t heap_memory_ops = {
//     .malloc = kmalloc,
//     .free = kfree,
// };
