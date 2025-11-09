#include "pmm.h"
#include <string.h>

// Глобальна структура PMM
static pmm_info_t g_pmm_info = {0};

// Початок heap-області (куди можемо розміщувати bitmap та виділяти сторінки)
static uint64_t g_heap_start = 0;
static uint64_t g_heap_end = 0;

// Для швидкого пошуку вільних сторінок
static uint64_t g_last_search_index = 0;

// === Bitmap Helper Functions ===

static inline void bitmap_set_bit(uint8_t *bitmap, uint64_t bit)
{
	bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear_bit(uint8_t *bitmap, uint64_t bit)
{
	bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool bitmap_test_bit(uint8_t *bitmap, uint64_t bit)
{
	return (bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

// === PMM Core Functions ===

void pmm_init(uint64_t heap_start, uint64_t heap_size)
{
	g_heap_start = heap_start;
	g_heap_end = heap_start + heap_size;

	// Обчислюємо кількість сторінок
	g_pmm_info.total_memory = heap_size;
	g_pmm_info.usable_memory = heap_size;
	g_pmm_info.total_pages = heap_size / PAGE_SIZE;

	// Обчислюємо розмір bitmap (1 біт на сторінку)
	g_pmm_info.bitmap_size = (g_pmm_info.total_pages + 7) / 8; // округлення вгору

	// Вирівнюємо розмір bitmap до сторінки
	g_pmm_info.bitmap_size = PAGE_ALIGN_UP(g_pmm_info.bitmap_size);

	// Розміщуємо bitmap на початку heap
	g_pmm_info.bitmap = (uint8_t *)heap_start;

	// Очищуємо bitmap (всі біти = 0 означає "вільно")
	memset(g_pmm_info.bitmap, 0, g_pmm_info.bitmap_size);

	// Позначаємо сторінки, які займає bitmap, як зайняті
	uint64_t bitmap_pages = g_pmm_info.bitmap_size / PAGE_SIZE;
	for (uint64_t i = 0; i < bitmap_pages; i++)
	{
		bitmap_set_bit(g_pmm_info.bitmap, i);
		g_pmm_info.used_pages++;
	}

	g_pmm_info.used_memory = g_pmm_info.used_pages * PAGE_SIZE;

	// Оновлюємо heap_start, щоб вказував після bitmap
	g_heap_start += g_pmm_info.bitmap_size;

	g_last_search_index = bitmap_pages;
}

uint64_t pmm_alloc_page(void)
{
	// Шукаємо першу вільну сторінку
	for (uint64_t i = g_last_search_index; i < g_pmm_info.total_pages; i++)
	{
		if (!bitmap_test_bit(g_pmm_info.bitmap, i))
		{
			// Знайшли вільну сторінку
			bitmap_set_bit(g_pmm_info.bitmap, i);
			g_pmm_info.used_pages++;
			g_pmm_info.used_memory += PAGE_SIZE;
			g_last_search_index = i + 1;

			// Обчислюємо фізичну адресу
			uint64_t phys_addr = (g_heap_start - g_pmm_info.bitmap_size) + (i * PAGE_SIZE);
			return phys_addr;
		}
	}

	// Якщо не знайшли, шукаємо з початку
	for (uint64_t i = 0; i < g_last_search_index; i++)
	{
		if (!bitmap_test_bit(g_pmm_info.bitmap, i))
		{
			bitmap_set_bit(g_pmm_info.bitmap, i);
			g_pmm_info.used_pages++;
			g_pmm_info.used_memory += PAGE_SIZE;
			g_last_search_index = i + 1;

			uint64_t phys_addr = (g_heap_start - g_pmm_info.bitmap_size) + (i * PAGE_SIZE);
			return phys_addr;
		}
	}

	// Немає вільних сторінок
	return 0;
}

uint64_t pmm_alloc_pages(size_t count)
{
	if (count == 0)
		return 0;
	if (count == 1)
		return pmm_alloc_page();

	// Шукаємо послідовні вільні сторінки
	uint64_t consecutive = 0;
	uint64_t start_index = 0;

	for (uint64_t i = 0; i < g_pmm_info.total_pages; i++)
	{
		if (!bitmap_test_bit(g_pmm_info.bitmap, i))
		{
			if (consecutive == 0)
			{
				start_index = i;
			}
			consecutive++;

			if (consecutive == count)
			{
				// Знайшли достатньо послідовних сторінок
				for (uint64_t j = 0; j < count; j++)
				{
					bitmap_set_bit(g_pmm_info.bitmap, start_index + j);
				}

				g_pmm_info.used_pages += count;
				g_pmm_info.used_memory += count * PAGE_SIZE;
				g_last_search_index = start_index + count;

				uint64_t phys_addr = (g_heap_start - g_pmm_info.bitmap_size) +
						     (start_index * PAGE_SIZE);
				return phys_addr;
			}
		}
		else
		{
			consecutive = 0;
		}
	}

	// Не знайшли достатньо послідовних сторінок
	return 0;
}

void pmm_free_page(uint64_t addr)
{
	// Перевіряємо вирівнювання
	if (addr % PAGE_SIZE != 0)
	{
		return;
	}

	// Обчислюємо індекс сторінки
	uint64_t base_addr = g_heap_start - g_pmm_info.bitmap_size;
	if (addr < base_addr || addr >= g_heap_end)
	{
		return; // Адреса поза межами heap
	}

	uint64_t page_index = (addr - base_addr) / PAGE_SIZE;

	if (page_index >= g_pmm_info.total_pages)
	{
		return;
	}

	// Звільняємо сторінку
	if (bitmap_test_bit(g_pmm_info.bitmap, page_index))
	{
		bitmap_clear_bit(g_pmm_info.bitmap, page_index);
		g_pmm_info.used_pages--;
		g_pmm_info.used_memory -= PAGE_SIZE;

		// Оновлюємо індекс пошуку для оптимізації
		if (page_index < g_last_search_index)
		{
			g_last_search_index = page_index;
		}
	}
}

void pmm_free_pages(uint64_t addr, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		pmm_free_page(addr + (i * PAGE_SIZE));
	}
}

void pmm_mark_page_used(uint64_t addr)
{
	if (addr % PAGE_SIZE != 0)
	{
		addr = PAGE_ALIGN_DOWN(addr);
	}

	uint64_t base_addr = g_heap_start - g_pmm_info.bitmap_size;
	if (addr < base_addr || addr >= g_heap_end)
	{
		return;
	}

	uint64_t page_index = (addr - base_addr) / PAGE_SIZE;

	if (page_index >= g_pmm_info.total_pages)
	{
		return;
	}

	if (!bitmap_test_bit(g_pmm_info.bitmap, page_index))
	{
		bitmap_set_bit(g_pmm_info.bitmap, page_index);
		g_pmm_info.used_pages++;
		g_pmm_info.used_memory += PAGE_SIZE;
	}
}

void pmm_mark_region_used(uint64_t addr, uint64_t size)
{
	uint64_t start = PAGE_ALIGN_DOWN(addr);
	uint64_t end = PAGE_ALIGN_UP(addr + size);

	for (uint64_t page = start; page < end; page += PAGE_SIZE)
	{
		pmm_mark_page_used(page);
	}
}

pmm_info_t *pmm_get_info(void)
{
	return &g_pmm_info;
}
