/**
 * @file slab.c
 * @brief Fixed Slab Allocator Implementation
 */

#include "slab.h"
#include "pmm.h"
#include "higher_half.h"
#include <string.h>

#include "printk.h"

// ============================================================================
// Global State
// ============================================================================

static slab_cache_t *g_cache_list = NULL;
static slab_info_t g_slab_info = {0};

static const size_t g_standard_sizes[] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048};
static const size_t g_standard_count = sizeof(g_standard_sizes) / sizeof(size_t);

static slab_cache_t *g_standard_caches[12] = {NULL};

// ============================================================================
// Memory Allocation Helpers (PMM-only)
// ============================================================================

static void *slab_alloc_page(void)
{
	uint64_t phys = pmm_alloc_page();
	if (phys == 0)
		return NULL;

	return (void *)PHYS_TO_VIRT(phys);
}

static void slab_free_page(void *addr)
{
	if (!addr)
		return;

	uint64_t phys = VIRT_TO_PHYS(addr);
	pmm_free_page(phys);
}

// ============================================================================
// Helper Functions
// ============================================================================

static inline size_t align_up(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

static inline bool is_power_of_2(size_t n)
{
	return (n != 0) && ((n & (n - 1)) == 0);
}

// Перевірка чи вказівник знаходиться в валідному діапазоні higher-half
static inline bool is_valid_kernel_ptr(void *ptr)
{
	uint64_t addr = (uint64_t)ptr;
	return (addr >= KERNEL_VIRT_BASE && addr != 0);
}

static slab_cache_t *find_cache(size_t size)
{
	for (size_t i = 0; i < g_standard_count; i++)
	{
		if (size <= g_standard_sizes[i])
		{
			return g_standard_caches[i];
		}
	}

	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		if (cache->object_size >= size)
		{
			return cache;
		}
		cache = cache->next;
	}

	return NULL;
}

// ============================================================================
// Slab Management
// ============================================================================

static slab_t *slab_create(slab_cache_t *cache)
{
	void *page = slab_alloc_page();
	if (!page)
		return NULL;

	// Обнуляємо всю сторінку для безпеки
	memset(page, 0, PAGE_SIZE);

	slab_t *slab = (slab_t *)page;

	size_t metadata_size = sizeof(slab_t);
	metadata_size = align_up(metadata_size, cache->align);

	slab->next = NULL;
	slab->prev = NULL;
	slab->in_use = 0;
	slab->capacity = cache->objects_per_slab;
	slab->start = (uint8_t *)page + metadata_size;

	// Ініціалізуємо free list
	void **free_ptr = (void **)slab->start;
	slab->free_list = free_ptr;

	for (uint32_t i = 0; i < cache->objects_per_slab - 1; i++)
	{
		void *next_obj = (uint8_t *)free_ptr + cache->object_size;
		*free_ptr = next_obj;
		free_ptr = (void **)next_obj;
	}
	*free_ptr = NULL;

	cache->total_slabs++;
	cache->total_objects += cache->objects_per_slab;
	g_slab_info.total_slabs++;
	g_slab_info.total_memory += PAGE_SIZE;

	return slab;
}

static void slab_destroy(slab_cache_t *cache, slab_t *slab)
{
	if (slab->prev)
	{
		slab->prev->next = slab->next;
	}
	if (slab->next)
	{
		slab->next->prev = slab->prev;
	}

	cache->total_slabs--;
	cache->total_objects -= slab->capacity;
	g_slab_info.total_slabs--;
	g_slab_info.total_memory -= PAGE_SIZE;

	slab_free_page(slab);
}

static void slab_add_to_list(slab_t **list, slab_t *slab)
{
	slab->next = *list;
	slab->prev = NULL;

	if (*list)
	{
		(*list)->prev = slab;
	}

	*list = slab;
}

static void slab_remove_from_list(slab_t **list, slab_t *slab)
{
	if (slab->prev)
	{
		slab->prev->next = slab->next;
	}
	else
	{
		*list = slab->next;
	}

	if (slab->next)
	{
		slab->next->prev = slab->prev;
	}

	slab->next = NULL;
	slab->prev = NULL;
}

// Безпечна перевірка чи вказівник належить до slab
static inline bool ptr_in_slab_range(void *ptr, slab_t *slab)
{
	uint64_t slab_start = (uint64_t)slab;
	uint64_t slab_end = slab_start + PAGE_SIZE;
	uint64_t obj_addr = (uint64_t)ptr;

	// Об'єкт має бути в межах сторінки slab'а
	return (obj_addr >= slab_start && obj_addr < slab_end);
}

// ВИПРАВЛЕНО: Безпечний пошук slab для об'єкта
static slab_t *find_slab_for_object(slab_cache_t *cache, void *ptr)
{
	if (!cache || !ptr || !is_valid_kernel_ptr(ptr) || !is_valid_kernel_ptr(cache))
		return NULL;

	// Перевіряємо що pointer вирівняний до розміру об'єкта
	uint64_t addr = (uint64_t)ptr;
	if ((addr & (cache->align - 1)) != 0)
	{
		printk(KERN_ERR "Unaligned pointer: %p (align=%zu)\n", ptr, cache->align);
		return NULL;
	}

	// Пошук у partial slabs (найбільш ймовірно)
	slab_t *slab = cache->slabs_partial;
	int safety = 0;
	while (slab && safety++ < 100)
	{
		if (!is_valid_kernel_ptr(slab))
			break;

		if (ptr_in_slab_range(ptr, slab))
			return slab;

		slab = slab->next;
	}

	// Пошук у full slabs
	slab = cache->slabs_full;
	safety = 0;
	while (slab && safety++ < 100)
	{
		if (!is_valid_kernel_ptr(slab))
			break;

		if (ptr_in_slab_range(ptr, slab))
			return slab;

		slab = slab->next;
	}

	return NULL;
}

// ============================================================================
// Cache Management
// ============================================================================

slab_cache_t *slab_cache_create(size_t size, size_t align)
{
	if (size == 0 || size > SLAB_MAX_SIZE)
	{
		return NULL;
	}

	if (align == 0 || !is_power_of_2(align))
	{
		align = 8;
	}

	// ВИПРАВЛЕНО: Виділяємо окрему сторінку для структури cache
	slab_cache_t *cache = (slab_cache_t *)slab_alloc_page();
	if (!cache)
	{
		return NULL;
	}

	// Обнуляємо всю структуру
	memset(cache, 0, PAGE_SIZE);

	cache->object_size = align_up(size, align);
	cache->align = align;

	// Обчислюємо кількість об'єктів на slab
	size_t metadata_size = align_up(sizeof(slab_t), align);
	size_t available = PAGE_SIZE - metadata_size;
	cache->objects_per_slab = available / cache->object_size;

	if (cache->objects_per_slab == 0)
	{
		slab_free_page(cache);
		return NULL;
	}

	// Додаємо в глобальний список
	cache->next = g_cache_list;
	g_cache_list = cache;
	g_slab_info.cache_count++;

	printk(KERN_DEBUG "Created cache: size=%zu, align=%zu, objs_per_slab=%u\n",
	       cache->object_size, cache->align, cache->objects_per_slab);

	return cache;
}

void *slab_cache_alloc(slab_cache_t *cache)
{
	if (!cache || !is_valid_kernel_ptr(cache))
	{
		return NULL;
	}

	slab_t *slab = NULL;

	if (cache->slabs_partial)
	{
		slab = cache->slabs_partial;
		g_slab_info.cache_hits++;
	}
	else if (cache->slabs_free)
	{
		slab = cache->slabs_free;
		slab_remove_from_list(&cache->slabs_free, slab);
		slab_add_to_list(&cache->slabs_partial, slab);
		g_slab_info.cache_hits++;
	}
	else
	{
		slab = slab_create(cache);
		if (!slab)
		{
			g_slab_info.cache_misses++;
			return NULL;
		}
		slab_add_to_list(&cache->slabs_partial, slab);
		g_slab_info.cache_misses++;
	}

	void *obj = slab->free_list;
	if (!obj)
	{
		printk(KERN_ERR "Slab free_list is NULL but slab is in partial list!\n");
		return NULL;
	}

	slab->free_list = *(void **)obj;
	slab->in_use++;

	if (slab->in_use == slab->capacity)
	{
		slab_remove_from_list(&cache->slabs_partial, slab);
		slab_add_to_list(&cache->slabs_full, slab);
	}

	cache->used_objects++;
	g_slab_info.total_allocations++;
	g_slab_info.used_memory += cache->object_size;

	return obj;
}

void slab_cache_free(slab_cache_t *cache, void *ptr)
{
	if (!cache || !ptr || !is_valid_kernel_ptr(cache) || !is_valid_kernel_ptr(ptr))
	{
		printk(KERN_ERR "slab_cache_free: invalid parameters\n");
		return;
	}

	slab_t *slab = find_slab_for_object(cache, ptr);
	if (!slab)
	{
		printk(KERN_ERR "slab_cache_free: object %p not found in cache\n", ptr);
		return;
	}

	bool was_full = (slab->in_use == slab->capacity);

	// Додаємо об'єкт назад у free list
	*(void **)ptr = slab->free_list;
	slab->free_list = ptr;
	slab->in_use--;

	if (was_full)
	{
		slab_remove_from_list(&cache->slabs_full, slab);
		slab_add_to_list(&cache->slabs_partial, slab);
	}
	else if (slab->in_use == 0)
	{
		slab_remove_from_list(&cache->slabs_partial, slab);
		slab_add_to_list(&cache->slabs_free, slab);
	}

	cache->used_objects--;
	g_slab_info.total_frees++;
	g_slab_info.used_memory -= cache->object_size;
}

void slab_cache_destroy(slab_cache_t *cache)
{
	if (!cache)
		return;

	while (cache->slabs_free)
	{
		slab_t *next = cache->slabs_free->next;
		slab_destroy(cache, cache->slabs_free);
		cache->slabs_free = next;
	}

	while (cache->slabs_partial)
	{
		slab_t *next = cache->slabs_partial->next;
		slab_destroy(cache, cache->slabs_partial);
		cache->slabs_partial = next;
	}

	while (cache->slabs_full)
	{
		slab_t *next = cache->slabs_full->next;
		slab_destroy(cache, cache->slabs_full);
		cache->slabs_full = next;
	}

	if (g_cache_list == cache)
	{
		g_cache_list = cache->next;
	}
	else
	{
		slab_cache_t *curr = g_cache_list;
		while (curr && curr->next != cache)
		{
			curr = curr->next;
		}
		if (curr)
		{
			curr->next = cache->next;
		}
	}

	g_slab_info.cache_count--;
	slab_free_page(cache);
}

uint32_t slab_cache_shrink(slab_cache_t *cache)
{
	if (!cache)
		return 0;

	uint32_t freed = 0;

	while (cache->slabs_free)
	{
		slab_t *next = cache->slabs_free->next;
		slab_destroy(cache, cache->slabs_free);
		cache->slabs_free = next;
		freed++;
	}

	return freed;
}

// ============================================================================
// Main API
// ============================================================================

void slab_init(void)
{
	printk(KERN_INFO "Initializing %zu standard slab caches...\n", g_standard_count);

	for (size_t i = 0; i < g_standard_count; i++)
	{
		g_standard_caches[i] = slab_cache_create(g_standard_sizes[i], 8);

		if (!g_standard_caches[i])
		{
			printk(KERN_ERR "FAILED to create cache for size %zu!\n",
			       g_standard_sizes[i]);
			return;
		}

		printk(KERN_DEBUG "  [%zu] size=%zu at %p\n",
		       i, g_standard_sizes[i], g_standard_caches[i]);
	}

	printk(KERN_INFO "Slab allocator initialized successfully\n");
}

void *slab_alloc(size_t size)
{
	if (size == 0 || size > SLAB_MAX_SIZE)
	{
		return NULL;
	}

	slab_cache_t *cache = find_cache(size);
	if (!cache)
	{
		printk(KERN_ERR "No suitable cache for size %zu\n", size);
		return NULL;
	}

	void *ptr = slab_cache_alloc(cache);

	if (ptr)
	{
		// printk(KERN_DEBUG "slab_alloc(%zu) -> %p (cache=%p, obj_size=%zu)\n",
		//        size, ptr, cache, cache->object_size);
	}

	return ptr;
}

void slab_free(void *ptr)
{
	if (!ptr)
		return;

	printk(KERN_DEBUG "slab_free(%p)\n", ptr);

	if (!is_valid_kernel_ptr(ptr))
	{
		printk(KERN_ERR "slab_free: invalid pointer %p\n", ptr);
		return;
	}

	// Спочатку шукаємо в стандартних кешах
	for (size_t i = 0; i < g_standard_count; i++)
	{
		slab_cache_t *cache = g_standard_caches[i];
		if (!cache || !is_valid_kernel_ptr(cache))
			continue;

		slab_t *slab = find_slab_for_object(cache, ptr);
		if (slab)
		{
			printk(KERN_DEBUG "  Found in standard cache[%zu]\n", i);
			slab_cache_free(cache, ptr);
			return;
		}
	}

	// Шукаємо в динамічних кешах
	slab_cache_t *cache = g_cache_list;
	int safety = 0;

	while (cache && safety++ < 100)
	{
		if (!is_valid_kernel_ptr(cache))
			break;

		slab_t *slab = find_slab_for_object(cache, ptr);
		if (slab)
		{
			printk(KERN_DEBUG "  Found in dynamic cache\n");
			slab_cache_free(cache, ptr);
			return;
		}

		cache = cache->next;
	}

	printk(KERN_WARNING "slab_free: pointer %p not found in any cache!\n", ptr);
}

void *slab_calloc(size_t size)
{
	void *ptr = slab_alloc(size);
	if (ptr)
	{
		memset(ptr, 0, size);
	}
	return ptr;
}

void *slab_realloc(void *ptr, size_t new_size)
{
	if (!ptr)
		return slab_alloc(new_size);

	if (new_size == 0)
	{
		slab_free(ptr);
		return NULL;
	}

	// Знаходимо поточний розмір
	size_t old_size = 0;

	for (size_t i = 0; i < g_standard_count; i++)
	{
		slab_cache_t *cache = g_standard_caches[i];
		if (cache && find_slab_for_object(cache, ptr))
		{
			old_size = cache->object_size;
			break;
		}
	}

	if (old_size == 0)
	{
		slab_cache_t *cache = g_cache_list;
		while (cache)
		{
			if (find_slab_for_object(cache, ptr))
			{
				old_size = cache->object_size;
				break;
			}
			cache = cache->next;
		}
	}

	if (old_size == 0)
		return NULL;

	if (new_size <= old_size)
		return ptr;

	void *new_ptr = slab_alloc(new_size);
	if (new_ptr)
	{
		memcpy(new_ptr, ptr, old_size);
		slab_free(ptr);
	}

	return new_ptr;
}

// ============================================================================
// Statistics
// ============================================================================

slab_info_t *slab_get_info(void)
{
	g_slab_info.wasted_memory = g_slab_info.total_memory - g_slab_info.used_memory;
	return &g_slab_info;
}

void slab_cache_info(slab_cache_t *cache)
{
	(void)cache;
}

void slab_print_caches(void)
{
}

bool slab_validate(void)
{
	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

		for (int i = 0; i < 3; i++)
		{
			slab_t *slab = lists[i];
			while (slab)
			{
				if (slab->capacity != cache->objects_per_slab)
					return false;

				if (slab->in_use > slab->capacity)
					return false;

				slab = slab->next;
			}
		}

		cache = cache->next;
	}

	return true;
}
