// #include <heap.h>

// #define PAGE_SIZE 0x1000 // 4KB

// static uint64_t heap_start_addr = 0;
// static uint64_t heap_end_addr = 0;
// static size_t total_pages = 0;
// static uint8_t *page_bitmap = NULL; // 1 bit per page

// // Bitmap
// static inline void set_page(size_t page) { page_bitmap[page / 8] |= 1 << (page % 8); }
// static inline void clear_page(size_t page) { page_bitmap[page / 8] &= ~(1 << (page % 8)); }
// static inline int test_page(size_t page) { return page_bitmap[page / 8] & (1 << (page % 8)); }

// typedef struct slab_block
// {
// 	struct slab_block *next;
// } slab_block_t;

// typedef struct slab_page
// {
// 	struct slab_page *next;
// 	slab_block_t *free_list;
// 	size_t free_count;
// 	size_t block_size;
// } slab_page_t;

// typedef struct slab_cache
// {
// 	size_t block_size;
// 	slab_page_t *pages;
// } slab_cache_t;

// #define NUM_BUCKETS 7
// size_t bucket_sizes[NUM_BUCKETS] = {16, 32, 64, 128, 256, 512, 1024};

// slab_cache_t slab_caches[NUM_BUCKETS];

// void slab_init()
// {
// 	for (int i = 0; i < NUM_BUCKETS; i++)
// 	{
// 		slab_caches[i].block_size = bucket_sizes[i];
// 		slab_caches[i].pages = NULL;
// 	}
// }

// // Initialize the heap
// void heap_init(uint64_t heap_start, uint64_t heap_size)
// {
// 	heap_start_addr = heap_start;
// 	heap_end_addr = heap_start + heap_size;

// 	total_pages = heap_size / PAGE_SIZE;

// 	// Bitmap in start of heap
// 	page_bitmap = (uint8_t *)heap_start_addr;
// 	size_t bitmap_size = (total_pages + 7) / 8;
// 	for (size_t i = 0; i < bitmap_size; i++)
// 		page_bitmap[i] = 0; // all bits are 0

// 	// Move heap_start_addr to bitmap
// 	heap_start_addr += bitmap_size;

// 	// Initialize slab caches
// 	slab_init();
// }

// void *alloc_page()
// {
// 	for (size_t i = 0; i < total_pages; i++)
// 	{
// 		if (!test_page(i))
// 		{
// 			set_page(i);
// 			return (void *)(heap_start_addr + i * PAGE_SIZE);
// 		}
// 	}
// 	return NULL; // No free pages
// }

// void *alloc_pages(size_t num_pages)
// {
// 	size_t consecutive = 0;
// 	for (size_t i = 0; i < total_pages; i++)
// 	{
// 		if (!test_page(i))
// 		{
// 			consecutive++;
// 			if (consecutive == num_pages)
// 			{
// 				size_t start_page = i + 1 - num_pages;
// 				for (size_t j = start_page; j <= i; j++)
// 					set_page(j);
// 				return (void *)(heap_start_addr + start_page * PAGE_SIZE);
// 			}
// 		}
// 		else
// 		{
// 			consecutive = 0;
// 		}
// 	}
// 	return NULL; // No free pages
// }

// void free_page(void *ptr)
// {
// 	size_t page = ((uint64_t)ptr - heap_start_addr) / PAGE_SIZE;
// 	clear_page(page);
// }

// void free_pages(void *ptr, size_t num_pages)
// {
// 	size_t start_page = ((uint64_t)ptr - heap_start_addr) / PAGE_SIZE;
// 	for (size_t i = 0; i < num_pages; i++)
// 		clear_page(start_page + i);
// }

// typedef struct page_block_header
// {
// 	size_t pages; // how many pages are in this block
// } page_block_header_t;

// slab_page_t *slab_alloc_page(size_t block_size)
// {
// 	void *page = alloc_page();
// 	if (!page)
// 		return NULL;

// 	slab_page_t *spage = (slab_page_t *)page;
// 	spage->next = NULL;
// 	spage->free_count = PAGE_SIZE / block_size;
// 	spage->free_list = (slab_block_t *)(spage + 1);
// 	spage->block_size = block_size;

// 	// Initialize free list
// 	uint8_t *block_ptr = (uint8_t *)spage->free_list;
// 	for (size_t i = 0; i < spage->free_count - 1; i++)
// 	{
// 		slab_block_t *b = (slab_block_t *)block_ptr;
// 		b->next = (slab_block_t *)(block_ptr + block_size);
// 		block_ptr += block_size;
// 	}
// 	((slab_block_t *)block_ptr)->next = NULL;

// 	return spage;
// }

// void *kmalloc(size_t size)
// {
// 	// Если больше одной страницы — используем старый страничный allocator
// 	if (size > 1024)
// 	{
// 		size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
// 		void *ptr = alloc_pages(pages_needed);
// 		if (!ptr)
// 			return NULL;
// 		return ptr;
// 	}

// 	// Выбираем подходящий бакет
// 	slab_cache_t *cache = NULL;
// 	for (int i = 0; i < NUM_BUCKETS; i++)
// 	{
// 		if (size <= slab_caches[i].block_size)
// 		{
// 			cache = &slab_caches[i];
// 			break;
// 		}
// 	}
// 	if (!cache)
// 		return NULL;

// 	// Найдем страницу с свободным блоком
// 	slab_page_t *page = cache->pages;
// 	while (page && !page->free_list)
// 		page = page->next;

// 	if (!page)
// 	{
// 		page = slab_alloc_page(cache->block_size);
// 		if (!page)
// 			return NULL;
// 		page->next = cache->pages;
// 		cache->pages = page;
// 	}

// 	// Выделяем блок
// 	slab_block_t *block = page->free_list;
// 	page->free_list = block->next;
// 	page->free_count--;

// 	return (void *)block;
// }

// void kfree(void *ptr)
// {
// 	if (!ptr)
// 		return;

// 	// Определяем страницу
// 	uintptr_t addr = (uintptr_t)ptr;
// 	slab_page_t *page = (slab_page_t *)(addr & ~(PAGE_SIZE - 1));

// 	// Проверяем, является ли это slab-блоком
// 	if (page->free_count + 1 <= PAGE_SIZE / 16) // минимальный блок size
// 	{
// 		// Случай slab
// 		((slab_block_t *)ptr)->next = page->free_list;
// 		page->free_list = (slab_block_t *)ptr;
// 		page->free_count++;

// 		// Опционально: если все блоки освобождены, можно вернуть страницу в bitmap
// 		if (page->free_count * page->next->block_size >= PAGE_SIZE)
// 		{
// 			// удалить страницу из кеша и free_page(page)
// 		}
// 	}
// 	else
// 	{
// 		// Страничная аллокация
// 		page_block_header_t *hdr = (page_block_header_t *)ptr;
// 		free_pages(ptr, hdr->pages);
// 	}
// }

// memory_ops_t heap_memory_ops = {
//     .malloc = kmalloc,
//     .free = kfree,
// };

// size_t count_used_pages()
// {
// 	size_t used = 0;
// 	for (size_t i = 0; i < total_pages; i++)
// 	{
// 		if (test_page(i))
// 			used++;
// 	}
// 	return used;
// }

// size_t count_free_pages()
// {
// 	return total_pages - count_used_pages();
// }

// #include <stdio.h>

// void print_memory_status()
// {
// 	size_t used_pages = count_used_pages();

// 	printf("Memory usage: %zu / %zu pages\n", used_pages, total_pages);
// }

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

typedef struct page_block_header
{
	size_t pages; // how many pages are in this block
} page_block_header_t;

void *kmalloc(size_t size)
{
	// size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	// if (pages_needed == 1)
	// 	return alloc_page();
	// else
	// 	return alloc_pages(pages_needed);

	size_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	void *ptr = (pages_needed == 1) ? alloc_page() : alloc_pages(pages_needed);
	if (!ptr)
		return NULL;

	page_block_header_t *header = (page_block_header_t *)ptr;
	header->pages = pages_needed;
	return (void *)(header + 1);
}

void kfree(void *ptr)
{
	// free_page(ptr);
	if (!ptr)
		return;

	page_block_header_t *header = (page_block_header_t *)ptr - 1;
	free_pages(header + 1, header->pages);
}

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

memory_ops_t heap_memory_ops = {
    .malloc = kmalloc,
    .free = kfree,
};

size_t count_used_pages()
{
	size_t used = 0;
	for (size_t i = 0; i < total_pages; i++)
	{
		if (test_page(i))
			used++;
	}
	return used;
}

size_t count_free_pages()
{
	return total_pages - count_used_pages();
}

#include <stdio.h>

void print_memory_status()
{
	size_t used_pages = count_used_pages();

	printf("Memory usage: %zu / %zu pages\n", used_pages, total_pages);
}
