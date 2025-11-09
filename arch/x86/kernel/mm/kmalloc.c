#include "kmalloc.h"
#include "pmm.h"
#include "vmm.h"
#include "printk.h"
#include <string.h>

// Конфигурация
#define MIN_BLOCK_SIZE 32 // Минимальный размер блока (2^5)
#define MIN_ORDER 5	  // log2(32)
#define MAX_ORDER 20	  // 2^20 * 1 byte = 1MB max block
#define NUM_ORDERS (MAX_ORDER - MIN_ORDER + 1)

// Heap region
static uint64_t heap_start = 0;
static uint64_t heap_size = 0;
static uint64_t heap_used = 0;

// Заголовок блока
typedef struct block_header
{
	struct block_header *next; // Следующий блок в free list
	uint32_t order;		   // Порядок блока (log2 размера)
	uint32_t magic;		   // Магическое число для проверки
	uint8_t free;		   // 1 = свободен, 0 = занят
	uint8_t padding[3];
} __attribute__((packed)) block_header_t;

#define BLOCK_MAGIC 0xBEEFCAFE
#define HEADER_SIZE sizeof(block_header_t)

// Free lists для каждого order
static block_header_t *free_lists[NUM_ORDERS];

// Статистика
static struct
{
	uint64_t total_allocs;
	uint64_t total_frees;
	uint64_t current_blocks;
	uint64_t failed_allocs;
	uint64_t splits;
	uint64_t merges;
} stats = {0};

// ========== Helper Functions ==========

// Получить order для заданного размера
static inline uint32_t size_to_order(size_t size)
{
	// Добавляем размер заголовка
	size += HEADER_SIZE;

	// Находим минимальный order
	uint32_t order = MIN_ORDER;
	size_t block_size = 1ULL << order;

	while (block_size < size && order < MAX_ORDER)
	{
		order++;
		block_size <<= 1;
	}

	return order;
}

// Получить размер блока по order
static inline size_t order_to_size(uint32_t order)
{
	return 1ULL << order;
}

// Проверить, является ли адрес валидным
static inline int is_valid_address(void *ptr)
{
	uint64_t addr = (uint64_t)ptr;
	return (addr >= heap_start && addr < heap_start + heap_size);
}

// Получить buddy адрес
static inline uint64_t get_buddy_address(uint64_t addr, uint32_t order)
{
	uint64_t offset = addr - heap_start;
	uint64_t buddy_offset = offset ^ (1ULL << order);
	return heap_start + buddy_offset;
}

// Найти блок в free list
static block_header_t *find_in_free_list(uint32_t order, uint64_t addr)
{
	if (order < MIN_ORDER || order > MAX_ORDER)
		return NULL;

	uint32_t index = order - MIN_ORDER;
	block_header_t *current = free_lists[index];

	while (current)
	{
		if ((uint64_t)current == addr)
			return current;
		current = current->next;
	}

	return NULL;
}

// Удалить блок из free list
static void remove_from_free_list(block_header_t *block)
{
	uint32_t index = block->order - MIN_ORDER;

	if (free_lists[index] == block)
	{
		free_lists[index] = block->next;
		return;
	}

	block_header_t *current = free_lists[index];
	while (current && current->next != block)
		current = current->next;

	if (current)
		current->next = block->next;
}

// Добавить блок в free list
static void add_to_free_list(block_header_t *block)
{
	uint32_t index = block->order - MIN_ORDER;
	block->next = free_lists[index];
	free_lists[index] = block;
	block->free = 1;
}

// ========== Core Allocation Functions ==========

// Разделить блок на два buddy
static block_header_t *split_block(block_header_t *block)
{
	if (block->order <= MIN_ORDER)
		return NULL;

	stats.splits++;

	// Уменьшаем order
	block->order--;

	// Создаём buddy блок
	uint64_t buddy_addr = (uint64_t)block + order_to_size(block->order);
	block_header_t *buddy = (block_header_t *)buddy_addr;

	buddy->order = block->order;
	buddy->magic = BLOCK_MAGIC;
	buddy->free = 1;
	buddy->next = NULL;

	// Добавляем buddy в free list
	add_to_free_list(buddy);

	return block;
}

// Найти свободный блок нужного order
static block_header_t *find_free_block(uint32_t order)
{
	// Ищем блок нужного размера
	for (uint32_t current_order = order; current_order <= MAX_ORDER; current_order++)
	{
		uint32_t index = current_order - MIN_ORDER;
		if (free_lists[index])
		{
			block_header_t *block = free_lists[index];

			// Удаляем из free list
			free_lists[index] = block->next;
			block->free = 0;

			// Разделяем блок до нужного размера
			while (block->order > order)
			{
				block = split_block(block);
				if (!block)
					return NULL;
			}

			return block;
		}
	}

	return NULL;
}

// Попытаться объединить с buddy
static block_header_t *try_merge_buddy(block_header_t *block)
{
	// Не можем объединять максимальный блок
	if (block->order >= MAX_ORDER)
		return block;

	uint64_t buddy_addr = get_buddy_address((uint64_t)block, block->order);

	// Проверяем валидность buddy адреса
	if (!is_valid_address((void *)buddy_addr))
		return block;

	// Ищем buddy в free list
	block_header_t *buddy = find_in_free_list(block->order, buddy_addr);

	if (!buddy || !buddy->free || buddy->magic != BLOCK_MAGIC)
		return block;

	stats.merges++;

	// Удаляем оба блока из free lists
	remove_from_free_list(block);
	remove_from_free_list(buddy);

	// Определяем какой блок будет родителем (с меньшим адресом)
	block_header_t *parent = ((uint64_t)block < buddy_addr) ? block : buddy;

	// Увеличиваем order
	parent->order++;
	parent->free = 1;
	parent->magic = BLOCK_MAGIC;

	// Рекурсивно пытаемся объединить дальше
	return try_merge_buddy(parent);
}

// ========== Public API ==========

void kmalloc_init(void)
{
	printk("=== kmalloc_init ===\n");

	// Получаем информацию о heap от PMM
	// Предполагаем, что есть функция получения heap региона
	// Для упрощения используем фиксированный регион

	// Выделяем большой блок от PMM (например, 16MB)
	size_t initial_heap_size = 16 * 1024 * 1024; // 16 MB
	size_t pages = initial_heap_size / PAGE_SIZE;

	void *heap_virt = pmm_alloc_phys(pages);
	if (!heap_virt)
	{
		printk("FATAL: Cannot allocate heap from PMM\n");
		return;
	}

	heap_start = (uint64_t)heap_virt;
	heap_size = initial_heap_size;

	printk("Heap region: 0x%lx - 0x%lx (%lu\n MB)\n",
	       heap_start, heap_start + heap_size, heap_size / (1024 * 1024));

	// Инициализируем free lists
	for (int i = 0; i < NUM_ORDERS; i++)
		free_lists[i] = NULL;

	// Создаём начальный максимальный блок
	block_header_t *initial = (block_header_t *)heap_start;
	initial->order = MAX_ORDER;
	initial->magic = BLOCK_MAGIC;
	initial->free = 1;
	initial->next = NULL;

	add_to_free_list(initial);

	// Разделяем на блоки поменьше
	uint64_t remaining = heap_size;
	uint64_t current_addr = heap_start;

	while (remaining > 0)
	{
		// Находим максимальный order для текущего региона
		uint32_t order = MAX_ORDER;
		while (order_to_size(order) > remaining && order > MIN_ORDER)
			order--;

		if (order_to_size(order) > remaining)
			break;

		block_header_t *block = (block_header_t *)current_addr;
		block->order = order;
		block->magic = BLOCK_MAGIC;
		block->free = 1;
		block->next = NULL;

		add_to_free_list(block);

		size_t block_size = order_to_size(order);
		current_addr += block_size;
		remaining -= block_size;
	}

	printk("kmalloc initialized with %d orders (%d - %d bytes)\n",
	       NUM_ORDERS, 1 << MIN_ORDER, 1 << MAX_ORDER);
}

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	stats.total_allocs++;

	uint32_t order = size_to_order(size);

	if (order > MAX_ORDER)
	{
		printk("kmalloc: size %lu\n too large (max %lu\n)\n",
		       size, (1ULL << MAX_ORDER) - HEADER_SIZE);
		stats.failed_allocs++;
		return NULL;
	}

	block_header_t *block = find_free_block(order);

	if (!block)
	{
		printk("kmalloc: out of memory (requested %lu\n bytes)\n", size);
		stats.failed_allocs++;
		return NULL;
	}

	block->free = 0;
	stats.current_blocks++;
	heap_used += order_to_size(block->order);

	// Возвращаем адрес после заголовка
	return (void *)((uint64_t)block + HEADER_SIZE);
}

void *kzalloc(size_t size)
{
	void *ptr = kmalloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	stats.total_frees++;

	// Получаем заголовок
	block_header_t *block = (block_header_t *)((uint64_t)ptr - HEADER_SIZE);

	// clear data
	memset(ptr, 0, order_to_size(block->order) - HEADER_SIZE);
	// Проверки валидности
	if (!is_valid_address(block))
	{
		printk("kfree: invalid pointer 0x%lx\n", (uint64_t)ptr);
		return;
	}

	if (block->magic != BLOCK_MAGIC)
	{
		printk("kfree: corrupted block (bad magic) at 0x%lx\n", (uint64_t)block);
		return;
	}

	if (block->free)
	{
		printk("kfree: double free detected at 0x%lx\n", (uint64_t)ptr);
		return;
	}

	stats.current_blocks--;
	heap_used -= order_to_size(block->order);

	block->free = 1;

	// Добавляем в free list
	add_to_free_list(block);

	// Пытаемся объединить с buddy
	try_merge_buddy(block);
}

void *krealloc(void *ptr, size_t new_size)
{
	if (!ptr)
		return kmalloc(new_size);

	if (new_size == 0)
	{
		kfree(ptr);
		return NULL;
	}

	block_header_t *block = (block_header_t *)((uint64_t)ptr - HEADER_SIZE);

	if (block->magic != BLOCK_MAGIC)
	{
		printk("krealloc: corrupted block\n");
		return NULL;
	}

	size_t old_size = order_to_size(block->order) - HEADER_SIZE;
	uint32_t new_order = size_to_order(new_size);

	// Если новый размер помещается в текущий блок
	if (new_order == block->order)
		return ptr;

	// Выделяем новый блок
	void *new_ptr = kmalloc(new_size);
	if (!new_ptr)
		return NULL;

	// Копируем данные
	size_t copy_size = (new_size < old_size) ? new_size : old_size;
	memcpy(new_ptr, ptr, copy_size);

	// Освобождаем старый блок
	kfree(ptr);

	return new_ptr;
}

void *kmalloc_aligned(size_t size, size_t alignment)
{
	if (alignment == 0 || (alignment & (alignment - 1)) != 0)
	{
		printk("kmalloc_aligned: alignment must be power of 2\n");
		return NULL;
	}

	// Выделяем больше памяти для выравнивания
	size_t total_size = size + alignment + HEADER_SIZE;
	void *ptr = kmalloc(total_size);

	if (!ptr)
		return NULL;

	// Выравниваем адрес
	uint64_t addr = (uint64_t)ptr;
	uint64_t aligned = (addr + alignment - 1) & ~(alignment - 1);

	return (void *)aligned;
}

void kmalloc_stats(void)
{
	printk("\n=== kmalloc Statistics ===\n");
	printk("Heap region: 0x%lx - 0x%lx\n", heap_start, heap_start + heap_size);
	printk("Total size: %lu\n KB\n", heap_size / 1024);
	printk("Used: %lu\n KB (%lu\n%%)\n",
	       heap_used / 1024,
	       heap_size > 0 ? (heap_used * 100 / heap_size) : 0);
	printk("Free: %lu\n KB\n", (heap_size - heap_used) / 1024);
	printk("\nOperations:\n");
	printk("  Total allocs: %lu\n\n", stats.total_allocs);
	printk("  Total frees: %lu\n\n", stats.total_frees);
	printk("  Failed allocs: %lu\n\n", stats.failed_allocs);
	printk("  Current blocks: %lu\n\n", stats.current_blocks);
	printk("  Splits: %lu\n\n", stats.splits);
	printk("  Merges: %lu\n\n", stats.merges);

	printk("\nFree lists:\n");
	for (int i = 0; i < NUM_ORDERS; i++)
	{
		uint32_t order = i + MIN_ORDER;
		int count = 0;
		block_header_t *current = free_lists[i];

		while (current)
		{
			count++;
			current = current->next;
		}

		if (count > 0)
		{
			printk("  Order %2d (%6llu bytes): %d blocks\n",
			       order, order_to_size(order), count);
		}
	}
	printk("==========================\n\n");
}

#include <mm/mm.h>

memory_ops_t heap_memory_ops = {
    .malloc = kmalloc,
    .realloc = krealloc,
    .free = kfree,
};
