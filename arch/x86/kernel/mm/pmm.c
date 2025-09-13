#include <mm/pmm.h>
#include <lib/bitmap.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define PAGE_SIZE 4096 // 4 KB

static uint64_t heap_start;
static uint64_t heap_size;

static uint64_t total_pages;
static uint8_t *bitmap;

void pmm_init(uint64_t start, uint64_t size)
{
	heap_start = start;
	heap_size = size;
	total_pages = heap_size / PAGE_SIZE;

	// Выделяем bitmap в начале heap (1 бит = 1 страница)
	size_t bitmap_size = (total_pages + 7) / 8;
	bitmap = (uint8_t *)heap_start;
	memset(bitmap, 0, bitmap_size);

	// Смещаем heap_start, чтобы оставшаяся память использовалась под страницы
	heap_start += bitmap_size;
}

// Выделение N подряд идущих страниц
void *pmm_alloc_pages(size_t n)
{
	if (n == 0 || n > total_pages)
		return NULL;

	for (size_t i = 0; i <= total_pages - n; i++)
	{
		// Проверяем, свободны ли все n страниц
		size_t j;
		for (j = 0; j < n; j++)
		{
			if (bitmap_test(bitmap, i + j))
				break;
		}
		if (j == n)
		{ // все свободны
			for (size_t k = 0; k < n; k++)
				bitmap_set(bitmap, i + k);
			return (void *)(heap_start + i * PAGE_SIZE);
		}
		i += j; // оптимизация — пропускаем занятые
	}
	return NULL; // нет подходящего блока
}

// Освобождение N страниц подряд
void pmm_free_pages(void *addr, size_t n)
{
	size_t index = ((uint64_t)addr - heap_start) / PAGE_SIZE;
	if (index + n <= total_pages)
	{
		for (size_t i = 0; i < n; i++)
			bitmap_reset(bitmap, index + i);
	}
}
