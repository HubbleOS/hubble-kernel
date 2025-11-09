#include "pmm.h"
#include "printk.h"
#include <string.h>

#define PAGE_SIZE 0x1000
#define PHYSMAP_BASE 0xFFFF800000000000ULL

static uint64_t heap_phys_start;
static uint64_t heap_size;
static uint64_t total_pages;
static uint64_t bitmap_phys;
static size_t bitmap_size;

static int physmap_available = 0;

// Всегда используем identity mapping для bitmap
// (т.к. bitmap находится в низкой памяти, которая всегда identity-mapped)
static inline uint8_t *get_bitmap(void)
{
	if (physmap_available)
	{
		// Используем physmap если доступен
		return (uint8_t *)(bitmap_phys + PHYSMAP_BASE);
	}
	else
	{
		// Используем identity mapping (bootstrap)
		return (uint8_t *)bitmap_phys;
	}
}

static inline void set_page(size_t page)
{
	uint8_t *bitmap = get_bitmap();
	bitmap[page / 8] |= 1 << (page % 8);
}

static inline void clear_page(size_t page)
{
	uint8_t *bitmap = get_bitmap();
	bitmap[page / 8] &= ~(1 << (page % 8));
}

static inline int test_page(size_t page)
{
	uint8_t *bitmap = get_bitmap();
	return bitmap[page / 8] & (1 << (page % 8));
}

void pmm_init(uint64_t heap_phys, uint64_t size)
{
	printk("=== PMM Init ===\n");

	// VMM уже зарезервировал последние 4MB для bootstrap allocator
	// Уменьшаем размер еще на 4MB чтобы не конфликтовать
	uint64_t bootstrap_reserve = 4 * 1024 * 1024;
	if (size > bootstrap_reserve)
	{
		size -= bootstrap_reserve;
	}

	printk("Physical region: 0x%lx - 0x%lx (%lu MB)\n",
	       heap_phys, heap_phys + size, size / (1024 * 1024));

	heap_phys_start = heap_phys;
	heap_size = size;
	total_pages = heap_size / PAGE_SIZE;

	bitmap_phys = heap_phys_start;
	bitmap_size = (total_pages + 7) / 8;

	printk("Bitmap: %lu bytes (%lu KB) at phys 0x%lx\n",
	       bitmap_size, bitmap_size / 1024, bitmap_phys);

	// На этом этапе physmap еще НЕ создан
	physmap_available = 0;

	// Очистка bitmap через identity mapping
	uint8_t *bitmap_virt = get_bitmap();
	memset(bitmap_virt, 0, bitmap_size);

	// Резервируем страницы под bitmap
	uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
	for (uint64_t i = 0; i < bitmap_pages; i++)
	{
		set_page(i);
	}

	// Обновляем доступное пространство
	heap_phys_start += bitmap_pages * PAGE_SIZE;
	heap_size -= bitmap_pages * PAGE_SIZE;
	total_pages = heap_size / PAGE_SIZE;

	printk("Reserved bitmap: %lu pages\n", bitmap_pages);
	printk("Usable: %lu pages (%lu MB)\n",
	       total_pages, total_pages * PAGE_SIZE / (1024 * 1024));
	printk("=== PMM Init Complete ===\n");
}

void pmm_enable_physmap(void)
{
	printk("PMM: Enabling physmap access\n");

	// Проверяем что physmap действительно работает
	uint8_t *test_identity = (uint8_t *)bitmap_phys;
	uint8_t *test_physmap = (uint8_t *)(bitmap_phys + PHYSMAP_BASE);

	uint8_t orig = test_identity[0];
	test_physmap[0] = 0xAB;

	if (test_identity[0] == 0xAB)
	{
		printk("PMM: Physmap verification OK\n");
		test_identity[0] = orig; // Восстанавливаем
		physmap_available = 1;
	}
	else
	{
		printk("PMM: WARNING - Physmap not working, staying with identity\n");
		test_identity[0] = orig;
	}
}

uint64_t pmm_alloc_phys(size_t pages)
{
	if (pages == 0)
		return 0;

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
				// Отмечаем как занятые
				for (size_t j = start_page; j < start_page + pages; j++)
					set_page(j);

				return heap_phys_start + start_page * PAGE_SIZE;
			}
		}
		else
		{
			consecutive = 0;
		}
	}

	return 0;
}

void pmm_free_phys(uint64_t phys_addr, size_t pages)
{
	if (phys_addr < heap_phys_start)
		return;

	size_t page = (phys_addr - heap_phys_start) / PAGE_SIZE;
	if (page >= total_pages)
		return;

	for (size_t i = 0; i < pages; i++)
	{
		if (page + i < total_pages)
			clear_page(page + i);
	}
}

size_t pmm_get_free_pages(void)
{
	size_t free = 0;
	for (size_t i = 0; i < total_pages; i++)
	{
		if (!test_page(i))
			free++;
	}
	return free;
}

size_t pmm_get_total_pages(void)
{
	return total_pages;
}
