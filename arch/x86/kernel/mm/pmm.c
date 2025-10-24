#include "pmm.h"
#include <string.h>

#define PAGE_SIZE 0x1000

static uint64_t heap_start_virt; // ВИРТУАЛЬНЫЙ адрес начала кучи
static uint64_t heap_start_phys; // ФИЗИЧЕСКИЙ адрес начала кучи
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

// ВАЖНО: pmm_start должен быть ФИЗИЧЕСКИМ адресом!
// Если у вас identity mapping, то virt == phys для низких адресов
void pmm_init(uint64_t pmm_start, uint64_t pmm_size)
{
	printf("=== PMM Init ===\n");
	printf("Physical region: 0x%llx - 0x%llx (%llu MB)\n",
	       pmm_start, pmm_start + pmm_size, pmm_size / (1024 * 1024));

	// ПРОВЕРКА: убеждаемся что регион доступен
	// Пробуем записать в начало
	volatile uint64_t *test = (volatile uint64_t *)pmm_start;
	*test = 0xDEADBEEF;
	if (*test != 0xDEADBEEF)
	{
		printf("FATAL: PMM region not writable!\n");
		return;
	}

	heap_start_phys = pmm_start;
	heap_start_virt = pmm_start;
	heap_size = pmm_size;

	total_pages = heap_size / PAGE_SIZE;
	bitmap = (uint8_t *)heap_start_virt;
	bitmap_size = (total_pages + 7) / 8;

	printf("Bitmap: %llu bytes (%llu KB)\n",
	       bitmap_size, bitmap_size / 1024);

	memset(bitmap, 0, bitmap_size);

	// Выравниваем начало на границу страницы
	uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
	heap_start_virt += bitmap_pages * PAGE_SIZE;
	heap_start_phys += bitmap_pages * PAGE_SIZE;
	heap_size -= bitmap_pages * PAGE_SIZE;
	total_pages = heap_size / PAGE_SIZE;

	printf("Usable: %llu pages (%llu MB)\n",
	       total_pages, total_pages * PAGE_SIZE / (1024 * 1024));
	printf("=== PMM Init Complete ===\n");
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

				// Возвращаем ВИРТУАЛЬНЫЙ адрес (identity mapped)
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

// Получить физический адрес из виртуального (для identity mapping)
uint64_t pmm_get_phys(void *virt_ptr)
{
	uint64_t offset = (uint64_t)virt_ptr - heap_start_virt;
	return heap_start_phys + offset;
}
