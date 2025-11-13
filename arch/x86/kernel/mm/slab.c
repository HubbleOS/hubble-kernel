// /**
//  * @file slab.c
//  * @brief Slab Allocator Implementation
//  */

// #include "slab.h"
// #include "pmm.h"
// #include "vmm.h"
// #include "higher_half.h"
// #include <string.h>

// // ============================================================================
// // Global State
// // ============================================================================

// static slab_cache_t *g_cache_list = NULL; // Список всіх кешів
// static slab_info_t g_slab_info = {0};	  // Статистика

// // Стандартні розміри кешів (степені 2)
// static const size_t g_standard_sizes[] = {
//     8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
// static const size_t g_standard_count = sizeof(g_standard_sizes) / sizeof(size_t);

// // Швидкий lookup для стандартних розмірів
// static slab_cache_t *g_standard_caches[12] = {NULL};

// // ============================================================================
// // Helper Functions
// // ============================================================================

// /**
//  * @brief Вирівнювання розміру
//  */
// static inline size_t align_up(size_t size, size_t align)
// {
// 	return (size + align - 1) & ~(align - 1);
// }

// /**
//  * @brief Перевірка чи є число степенем 2
//  */
// static inline bool is_power_of_2(size_t n)
// {
// 	return (n != 0) && ((n & (n - 1)) == 0);
// }

// /**
//  * @brief Знайти найближчий більший степінь 2
//  */
// static size_t next_power_of_2(size_t n)
// {
// 	if (n <= 8)
// 		return 8;

// 	n--;
// 	n |= n >> 1;
// 	n |= n >> 2;
// 	n |= n >> 4;
// 	n |= n >> 8;
// 	n |= n >> 16;
// 	n |= n >> 32;
// 	n++;

// 	return n;
// }

// /**
//  * @brief Знайти кеш для заданого розміру
//  */
// static slab_cache_t *find_cache(size_t size)
// {
// 	// Швидкий lookup для стандартних розмірів
// 	for (size_t i = 0; i < g_standard_count; i++)
// 	{
// 		if (size <= g_standard_sizes[i])
// 		{
// 			return g_standard_caches[i];
// 		}
// 	}

// 	// Пошук серед custom кешів
// 	slab_cache_t *cache = g_cache_list;
// 	while (cache)
// 	{
// 		if (cache->object_size >= size)
// 		{
// 			return cache;
// 		}
// 		cache = cache->next;
// 	}

// 	return NULL;
// }

// // ============================================================================
// // Slab Management
// // ============================================================================

// /**
//  * @brief Створити новий slab
//  */
// static slab_t *slab_create(slab_cache_t *cache)
// {
// 	// Виділяємо сторінку для slab
// 	void *page = vmm_alloc_kernel_pages(1);
// 	if (!page)
// 	{
// 		return NULL;
// 	}

// 	slab_t *slab = (slab_t *)page;

// 	// Метадані slab розміщуємо на початку сторінки
// 	size_t metadata_size = sizeof(slab_t);
// 	metadata_size = align_up(metadata_size, cache->align);

// 	slab->next = NULL;
// 	slab->prev = NULL;
// 	slab->in_use = 0;
// 	slab->capacity = cache->objects_per_slab;
// 	slab->start = (uint8_t *)page + metadata_size;

// 	// Ініціалізуємо free list
// 	void **free_ptr = (void **)slab->start;
// 	slab->free_list = free_ptr;

// 	for (uint32_t i = 0; i < cache->objects_per_slab - 1; i++)
// 	{
// 		void *next_obj = (uint8_t *)free_ptr + cache->object_size;
// 		*free_ptr = next_obj;
// 		free_ptr = (void **)next_obj;
// 	}
// 	*free_ptr = NULL; // Останній об'єкт

// 	cache->total_slabs++;
// 	cache->total_objects += cache->objects_per_slab;
// 	g_slab_info.total_slabs++;
// 	g_slab_info.total_memory += PAGE_SIZE;

// 	return slab;
// }

// /**
//  * @brief Видалити slab
//  */
// static void slab_destroy(slab_cache_t *cache, slab_t *slab)
// {
// 	// Видаляємо зі списку
// 	if (slab->prev)
// 	{
// 		slab->prev->next = slab->next;
// 	}
// 	if (slab->next)
// 	{
// 		slab->next->prev = slab->prev;
// 	}

// 	cache->total_slabs--;
// 	cache->total_objects -= slab->capacity;
// 	g_slab_info.total_slabs--;
// 	g_slab_info.total_memory -= PAGE_SIZE;

// 	// Звільняємо сторінку
// 	vmm_free_kernel_pages(slab, 1);
// }

// /**
//  * @brief Додати slab до списку
//  */
// static void slab_add_to_list(slab_t **list, slab_t *slab)
// {
// 	slab->next = *list;
// 	slab->prev = NULL;

// 	if (*list)
// 	{
// 		(*list)->prev = slab;
// 	}

// 	*list = slab;
// }

// /**
//  * @brief Видалити slab зі списку
//  */
// static void slab_remove_from_list(slab_t **list, slab_t *slab)
// {
// 	if (slab->prev)
// 	{
// 		slab->prev->next = slab->next;
// 	}
// 	else
// 	{
// 		*list = slab->next;
// 	}

// 	if (slab->next)
// 	{
// 		slab->next->prev = slab->prev;
// 	}

// 	slab->next = NULL;
// 	slab->prev = NULL;
// }

// /**
//  * @brief Знайти slab, який містить даний об'єкт
//  */
// static slab_t *find_slab_for_object(slab_cache_t *cache, void *ptr)
// {
// 	// Перевіряємо всі списки
// 	slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

// 	for (int i = 0; i < 3; i++)
// 	{
// 		slab_t *slab = lists[i];
// 		while (slab)
// 		{
// 			// Об'єкт має бути в межах slab сторінки
// 			uint64_t slab_start = (uint64_t)slab;
// 			uint64_t slab_end = slab_start + PAGE_SIZE;
// 			uint64_t obj_addr = (uint64_t)ptr;

// 			if (obj_addr >= slab_start && obj_addr < slab_end)
// 			{
// 				return slab;
// 			}

// 			slab = slab->next;
// 		}
// 	}

// 	return NULL;
// }

// // ============================================================================
// // Cache Management
// // ============================================================================

// slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align)
// {
// 	// Валідація параметрів
// 	if (size == 0 || size > SLAB_MAX_SIZE)
// 	{
// 		return NULL;
// 	}

// 	if (align == 0 || !is_power_of_2(align))
// 	{
// 		align = 8; // Default alignment
// 	}

// 	// Виділяємо структуру кешу
// 	slab_cache_t *cache = (slab_cache_t *)vmm_alloc_kernel_pages(1);
// 	if (!cache)
// 	{
// 		return NULL;
// 	}

// 	memset(cache, 0, sizeof(slab_cache_t));

// 	// Ініціалізуємо параметри
// 	if (name)
// 	{
// 		strncpy(cache->name, name, SLAB_NAME_MAX - 1);
// 		cache->name[SLAB_NAME_MAX - 1] = '\0';
// 	}

// 	cache->object_size = align_up(size, align);
// 	cache->align = align;

// 	// Обчислюємо кількість об'єктів на slab
// 	size_t metadata_size = align_up(sizeof(slab_t), align);
// 	size_t available = PAGE_SIZE - metadata_size;
// 	cache->objects_per_slab = available / cache->object_size;

// 	if (cache->objects_per_slab == 0)
// 	{
// 		vmm_free_kernel_pages(cache, 1);
// 		return NULL;
// 	}

// 	// Додаємо до глобального списку
// 	cache->next = g_cache_list;
// 	g_cache_list = cache;
// 	g_slab_info.cache_count++;

// 	return cache;
// }

// void *slab_cache_alloc(slab_cache_t *cache)
// {
// 	if (!cache)
// 	{
// 		return NULL;
// 	}

// 	slab_t *slab = NULL;

// 	// Шукаємо частково зайнятий slab
// 	if (cache->slabs_partial)
// 	{
// 		slab = cache->slabs_partial;
// 		g_slab_info.cache_hits++;
// 	}
// 	// Або вільний slab
// 	else if (cache->slabs_free)
// 	{
// 		slab = cache->slabs_free;
// 		slab_remove_from_list(&cache->slabs_free, slab);
// 		slab_add_to_list(&cache->slabs_partial, slab);
// 		g_slab_info.cache_hits++;
// 	}
// 	// Потрібно створити новий slab
// 	else
// 	{
// 		slab = slab_create(cache);
// 		if (!slab)
// 		{
// 			g_slab_info.cache_misses++;
// 			return NULL;
// 		}
// 		slab_add_to_list(&cache->slabs_partial, slab);
// 		g_slab_info.cache_misses++;
// 	}

// 	// Виділяємо об'єкт з free list
// 	void *obj = slab->free_list;
// 	if (!obj)
// 	{
// 		return NULL; // Помилка: slab має бути з вільними об'єктами
// 	}

// 	slab->free_list = *(void **)obj;
// 	slab->in_use++;

// 	// Якщо slab заповнився, переміщуємо його до full списку
// 	if (slab->in_use == slab->capacity)
// 	{
// 		slab_remove_from_list(&cache->slabs_partial, slab);
// 		slab_add_to_list(&cache->slabs_full, slab);
// 	}

// 	cache->used_objects++;
// 	g_slab_info.total_allocations++;
// 	g_slab_info.used_memory += cache->object_size;

// 	return obj;
// }

// void slab_cache_free(slab_cache_t *cache, void *ptr)
// {
// 	if (!cache || !ptr)
// 	{
// 		return;
// 	}

// 	// Знаходимо slab
// 	slab_t *slab = find_slab_for_object(cache, ptr);
// 	if (!slab)
// 	{
// 		return; // Об'єкт не належить до цього кешу
// 	}

// 	// Перевіряємо стан slab
// 	bool was_full = (slab->in_use == slab->capacity);

// 	// Додаємо об'єкт до free list
// 	*(void **)ptr = slab->free_list;
// 	slab->free_list = ptr;
// 	slab->in_use--;

// 	// Якщо slab був повний, переміщуємо його до partial
// 	if (was_full)
// 	{
// 		slab_remove_from_list(&cache->slabs_full, slab);
// 		slab_add_to_list(&cache->slabs_partial, slab);
// 	}
// 	// Якщо slab став порожнім, переміщуємо до free
// 	else if (slab->in_use == 0)
// 	{
// 		slab_remove_from_list(&cache->slabs_partial, slab);
// 		slab_add_to_list(&cache->slabs_free, slab);
// 	}

// 	cache->used_objects--;
// 	g_slab_info.total_frees++;
// 	g_slab_info.used_memory -= cache->object_size;
// }

// void slab_cache_destroy(slab_cache_t *cache)
// {
// 	if (!cache)
// 	{
// 		return;
// 	}

// 	// Звільняємо всі slabs
// 	while (cache->slabs_free)
// 	{
// 		slab_t *next = cache->slabs_free->next;
// 		slab_destroy(cache, cache->slabs_free);
// 		cache->slabs_free = next;
// 	}

// 	while (cache->slabs_partial)
// 	{
// 		slab_t *next = cache->slabs_partial->next;
// 		slab_destroy(cache, cache->slabs_partial);
// 		cache->slabs_partial = next;
// 	}

// 	while (cache->slabs_full)
// 	{
// 		slab_t *next = cache->slabs_full->next;
// 		slab_destroy(cache, cache->slabs_full);
// 		cache->slabs_full = next;
// 	}

// 	// Видаляємо з глобального списку
// 	if (g_cache_list == cache)
// 	{
// 		g_cache_list = cache->next;
// 	}
// 	else
// 	{
// 		slab_cache_t *curr = g_cache_list;
// 		while (curr && curr->next != cache)
// 		{
// 			curr = curr->next;
// 		}
// 		if (curr)
// 		{
// 			curr->next = cache->next;
// 		}
// 	}

// 	g_slab_info.cache_count--;

// 	// Звільняємо саму структуру кешу
// 	vmm_free_kernel_pages(cache, 1);
// }

// uint32_t slab_cache_shrink(slab_cache_t *cache)
// {
// 	if (!cache)
// 	{
// 		return 0;
// 	}

// 	uint32_t freed = 0;

// 	// Звільняємо тільки повністю порожні slabs
// 	while (cache->slabs_free)
// 	{
// 		slab_t *next = cache->slabs_free->next;
// 		slab_destroy(cache, cache->slabs_free);
// 		cache->slabs_free = next;
// 		freed++;
// 	}

// 	return freed;
// }

// // ============================================================================
// // Main API
// // ============================================================================

// void slab_init(void)
// {
// 	memset(&g_slab_info, 0, sizeof(slab_info_t));

// 	// Створюємо стандартні кеші
// 	for (size_t i = 0; i < g_standard_count; i++)
// 	{
// 		char name[32];
// 		snprintf(name, sizeof(name), "kmalloc-%lu", g_standard_sizes[i]);

// 		g_standard_caches[i] = slab_cache_create(
// 		    name,
// 		    g_standard_sizes[i],
// 		    8);
// 	}
// }

// void *slab_alloc(size_t size)
// {
// 	if (size == 0 || size > SLAB_MAX_SIZE)
// 	{
// 		return NULL;
// 	}

// 	// Знаходимо підходящий кеш
// 	slab_cache_t *cache = find_cache(size);
// 	if (!cache)
// 	{
// 		return NULL;
// 	}

// 	return slab_cache_alloc(cache);
// }

// void slab_free(void *ptr)
// {
// 	if (!ptr)
// 	{
// 		return;
// 	}

// 	// Знаходимо кеш через перебір всіх кешів
// 	// (в реальності краще зберігати метадані з кожним об'єктом)
// 	slab_cache_t *cache = g_cache_list;
// 	while (cache)
// 	{
// 		slab_t *slab = find_slab_for_object(cache, ptr);
// 		if (slab)
// 		{
// 			slab_cache_free(cache, ptr);
// 			return;
// 		}
// 		cache = cache->next;
// 	}

// 	// Також перевіряємо стандартні кеші
// 	for (size_t i = 0; i < g_standard_count; i++)
// 	{
// 		if (g_standard_caches[i])
// 		{
// 			slab_t *slab = find_slab_for_object(g_standard_caches[i], ptr);
// 			if (slab)
// 			{
// 				slab_cache_free(g_standard_caches[i], ptr);
// 				return;
// 			}
// 		}
// 	}
// }

// void *slab_calloc(size_t size)
// {
// 	void *ptr = slab_alloc(size);
// 	if (ptr)
// 	{
// 		memset(ptr, 0, size);
// 	}
// 	return ptr;
// }

// void *slab_realloc(void *ptr, size_t new_size)
// {
// 	if (!ptr)
// 	{
// 		return slab_alloc(new_size);
// 	}

// 	if (new_size == 0)
// 	{
// 		slab_free(ptr);
// 		return NULL;
// 	}

// 	// Знаходимо старий розмір (через кеш)
// 	size_t old_size = 0;
// 	slab_cache_t *cache = g_cache_list;
// 	while (cache)
// 	{
// 		slab_t *slab = find_slab_for_object(cache, ptr);
// 		if (slab)
// 		{
// 			old_size = cache->object_size;
// 			break;
// 		}
// 		cache = cache->next;
// 	}

// 	if (old_size == 0)
// 	{
// 		return NULL; // Не знайшли об'єкт
// 	}

// 	// Якщо новий розмір вміщається в старий кеш, повертаємо той самий вказівник
// 	if (new_size <= old_size)
// 	{
// 		return ptr;
// 	}

// 	// Інакше виділяємо новий об'єкт і копіюємо дані
// 	void *new_ptr = slab_alloc(new_size);
// 	if (new_ptr)
// 	{
// 		memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
// 		slab_free(ptr);
// 	}

// 	return new_ptr;
// }

// // ============================================================================
// // Statistics and Debugging
// // ============================================================================

// slab_info_t *slab_get_info(void)
// {
// 	// Оновлюємо статистику
// 	g_slab_info.wasted_memory = g_slab_info.total_memory - g_slab_info.used_memory;

// 	return &g_slab_info;
// }

// void slab_cache_info(slab_cache_t *cache)
// {
// 	if (!cache)
// 	{
// 		return;
// 	}

// 	// Требує printk
// 	// printk(KERN_INFO "Cache: %s\n", cache->name);
// 	// printk(KERN_INFO "  Object size: %lu bytes\n", cache->object_size);
// 	// printk(KERN_INFO "  Objects per slab: %u\n", cache->objects_per_slab);
// 	// printk(KERN_INFO "  Total slabs: %u\n", cache->total_slabs);
// 	// printk(KERN_INFO "  Total objects: %u\n", cache->total_objects);
// 	// printk(KERN_INFO "  Used objects: %u (%.1f%%)\n",
// 	//        cache->used_objects,
// 	//        (cache->total_objects > 0) ?
// 	//        (cache->used_objects * 100.0 / cache->total_objects) : 0.0);
// }

// void slab_print_caches(void)
// {
// 	// printk(KERN_INFO "\n=== Slab Caches ===\n");

// 	slab_cache_t *cache = g_cache_list;
// 	while (cache)
// 	{
// 		slab_cache_info(cache);
// 		cache = cache->next;
// 	}
// }

// bool slab_validate(void)
// {
// 	// Перевірка консистентності структур
// 	slab_cache_t *cache = g_cache_list;
// 	while (cache)
// 	{
// 		// Перевіряємо кожен slab
// 		slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

// 		for (int i = 0; i < 3; i++)
// 		{
// 			slab_t *slab = lists[i];
// 			while (slab)
// 			{
// 				// Перевірка capacity
// 				if (slab->capacity != cache->objects_per_slab)
// 				{
// 					return false;
// 				}

// 				// Перевірка in_use
// 				if (slab->in_use > slab->capacity)
// 				{
// 					return false;
// 				}

// 				slab = slab->next;
// 			}
// 		}

// 		cache = cache->next;
// 	}

// 	return true;
// }

/**
 * @file slab.c
 * @brief Slab Allocator Implementation
 */

#include "slab.h"
#include "pmm.h"
#include "vmm.h"
#include "higher_half.h"
#include <string.h>

// ============================================================================
// Helper Functions for String Formatting (without snprintf)
// ============================================================================

/**
 * @brief Simple integer to string conversion
 */
static void int_to_string(char *buf, size_t bufsize, uint64_t num)
{
	if (bufsize == 0)
		return;

	char temp[32];
	int i = 0;

	if (num == 0)
	{
		buf[0] = '0';
		buf[1] = '\0';
		return;
	}

	while (num > 0 && i < 31)
	{
		temp[i++] = '0' + (num % 10);
		num /= 10;
	}

	int j = 0;
	while (i > 0 && j < bufsize - 1)
	{
		buf[j++] = temp[--i];
	}
	buf[j] = '\0';
}

/**
 * @brief Create cache name without snprintf
 */
static void make_cache_name(char *dest, size_t dest_size, const char *prefix, size_t size)
{
	if (dest_size == 0)
		return;

	// Copy prefix
	size_t i = 0;
	while (prefix && *prefix && i < dest_size - 1)
	{
		dest[i++] = *prefix++;
	}

	// Add dash
	if (i < dest_size - 1)
		dest[i++] = '-';

	// Add number
	char num_buf[32];
	int_to_string(num_buf, sizeof(num_buf), size);

	size_t j = 0;
	while (num_buf[j] && i < dest_size - 1)
	{
		dest[i++] = num_buf[j++];
	}

	dest[i] = '\0';
}

// ============================================================================
// Global State
// ============================================================================

static slab_cache_t *g_cache_list = NULL; // Список всіх кешів
static slab_info_t g_slab_info = {0};	  // Статистика

// Стандартні розміри кешів (степені 2)
static const size_t g_standard_sizes[] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
static const size_t g_standard_count = sizeof(g_standard_sizes) / sizeof(size_t);

// Швидкий lookup для стандартних розмірів
static slab_cache_t *g_standard_caches[12] = {NULL};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Вирівнювання розміру
 */
static inline size_t align_up(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

/**
 * @brief Перевірка чи є число степенем 2
 */
static inline bool is_power_of_2(size_t n)
{
	return (n != 0) && ((n & (n - 1)) == 0);
}

/**
 * @brief Знайти найближчий більший степінь 2
 */
static size_t next_power_of_2(size_t n)
{
	if (n <= 8)
		return 8;

	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n |= n >> 32;
	n++;

	return n;
}

/**
 * @brief Знайти кеш для заданого розміру
 */
static slab_cache_t *find_cache(size_t size)
{
	// Швидкий lookup для стандартних розмірів
	for (size_t i = 0; i < g_standard_count; i++)
	{
		if (size <= g_standard_sizes[i])
		{
			return g_standard_caches[i];
		}
	}

	// Пошук серед custom кешів
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

/**
 * @brief Створити новий slab
 */
static slab_t *slab_create(slab_cache_t *cache)
{
	// Виділяємо сторінку для slab
	void *page = vmm_alloc_kernel_pages(1);
	if (!page)
	{
		return NULL;
	}

	slab_t *slab = (slab_t *)page;

	// Метадані slab розміщуємо на початку сторінки
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
	*free_ptr = NULL; // Останній об'єкт

	cache->total_slabs++;
	cache->total_objects += cache->objects_per_slab;
	g_slab_info.total_slabs++;
	g_slab_info.total_memory += PAGE_SIZE;

	return slab;
}

/**
 * @brief Видалити slab
 */
static void slab_destroy(slab_cache_t *cache, slab_t *slab)
{
	// Видаляємо зі списку
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

	// Звільняємо сторінку
	vmm_free_kernel_pages(slab, 1);
}

/**
 * @brief Додати slab до списку
 */
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

/**
 * @brief Видалити slab зі списку
 */
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

/**
 * @brief Знайти slab, який містить даний об'єкт
 */
static slab_t *find_slab_for_object(slab_cache_t *cache, void *ptr)
{
	// Перевіряємо всі списки
	slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

	for (int i = 0; i < 3; i++)
	{
		slab_t *slab = lists[i];
		while (slab)
		{
			// Об'єкт має бути в межах slab сторінки
			uint64_t slab_start = (uint64_t)slab;
			uint64_t slab_end = slab_start + PAGE_SIZE;
			uint64_t obj_addr = (uint64_t)ptr;

			if (obj_addr >= slab_start && obj_addr < slab_end)
			{
				return slab;
			}

			slab = slab->next;
		}
	}

	return NULL;
}

// ============================================================================
// Cache Management
// ============================================================================

slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align)
{
	// Валідація параметрів
	if (size == 0 || size > SLAB_MAX_SIZE)
	{
		return NULL;
	}

	if (align == 0 || !is_power_of_2(align))
	{
		align = 8; // Default alignment
	}

	// Виділяємо структуру кешу
	slab_cache_t *cache = (slab_cache_t *)vmm_alloc_kernel_pages(1);
	if (!cache)
	{
		return NULL;
	}

	memset(cache, 0, sizeof(slab_cache_t));

	// Ініціалізуємо параметри
	if (name)
	{
		size_t i = 0;
		while (name[i] && i < SLAB_NAME_MAX - 1)
		{
			cache->name[i] = name[i];
			i++;
		}
		cache->name[i] = '\0';
	}

	cache->object_size = align_up(size, align);
	cache->align = align;

	// Обчислюємо кількість об'єктів на slab
	size_t metadata_size = align_up(sizeof(slab_t), align);
	size_t available = PAGE_SIZE - metadata_size;
	cache->objects_per_slab = available / cache->object_size;

	if (cache->objects_per_slab == 0)
	{
		vmm_free_kernel_pages(cache, 1);
		return NULL;
	}

	// Додаємо до глобального списку
	cache->next = g_cache_list;
	g_cache_list = cache;
	g_slab_info.cache_count++;

	return cache;
}

void *slab_cache_alloc(slab_cache_t *cache)
{
	if (!cache)
	{
		return NULL;
	}

	slab_t *slab = NULL;

	// Шукаємо частково зайнятий slab
	if (cache->slabs_partial)
	{
		slab = cache->slabs_partial;
		g_slab_info.cache_hits++;
	}
	// Або вільний slab
	else if (cache->slabs_free)
	{
		slab = cache->slabs_free;
		slab_remove_from_list(&cache->slabs_free, slab);
		slab_add_to_list(&cache->slabs_partial, slab);
		g_slab_info.cache_hits++;
	}
	// Потрібно створити новий slab
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

	// Виділяємо об'єкт з free list
	void *obj = slab->free_list;
	if (!obj)
	{
		return NULL; // Помилка: slab має бути з вільними об'єктами
	}

	slab->free_list = *(void **)obj;
	slab->in_use++;

	// Якщо slab заповнився, переміщуємо його до full списку
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
	if (!cache || !ptr)
	{
		return;
	}

	// Знаходимо slab
	slab_t *slab = find_slab_for_object(cache, ptr);
	if (!slab)
	{
		return; // Об'єкт не належить до цього кешу
	}

	// Перевіряємо стан slab
	bool was_full = (slab->in_use == slab->capacity);

	// Додаємо об'єкт до free list
	*(void **)ptr = slab->free_list;
	slab->free_list = ptr;
	slab->in_use--;

	// Якщо slab був повний, переміщуємо його до partial
	if (was_full)
	{
		slab_remove_from_list(&cache->slabs_full, slab);
		slab_add_to_list(&cache->slabs_partial, slab);
	}
	// Якщо slab став порожнім, переміщуємо до free
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
	{
		return;
	}

	// Звільняємо всі slabs
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

	// Видаляємо з глобального списку
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

	// Звільняємо саму структуру кешу
	vmm_free_kernel_pages(cache, 1);
}

uint32_t slab_cache_shrink(slab_cache_t *cache)
{
	if (!cache)
	{
		return 0;
	}

	uint32_t freed = 0;

	// Звільняємо тільки повністю порожні slabs
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
	memset(&g_slab_info, 0, sizeof(slab_info_t));

	// Створюємо стандартні кеші
	for (size_t i = 0; i < g_standard_count; i++)
	{
		char name[32];
		make_cache_name(name, sizeof(name), "kmalloc", g_standard_sizes[i]);

		g_standard_caches[i] = slab_cache_create(
		    name,
		    g_standard_sizes[i],
		    8);
	}
}

void *slab_alloc(size_t size)
{
	if (size == 0 || size > SLAB_MAX_SIZE)
	{
		return NULL;
	}

	// Знаходимо підходящий кеш
	slab_cache_t *cache = find_cache(size);
	if (!cache)
	{
		return NULL;
	}

	return slab_cache_alloc(cache);
}

void slab_free(void *ptr)
{
	if (!ptr)
	{
		return;
	}

	// Знаходимо кеш через перебір всіх кешів
	// (в реальності краще зберігати метадані з кожним об'єктом)
	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		slab_t *slab = find_slab_for_object(cache, ptr);
		if (slab)
		{
			slab_cache_free(cache, ptr);
			return;
		}
		cache = cache->next;
	}

	// Також перевіряємо стандартні кеші
	for (size_t i = 0; i < g_standard_count; i++)
	{
		if (g_standard_caches[i])
		{
			slab_t *slab = find_slab_for_object(g_standard_caches[i], ptr);
			if (slab)
			{
				slab_cache_free(g_standard_caches[i], ptr);
				return;
			}
		}
	}
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
	{
		return slab_alloc(new_size);
	}

	if (new_size == 0)
	{
		slab_free(ptr);
		return NULL;
	}

	// Знаходимо старий розмір (через кеш)
	size_t old_size = 0;
	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		slab_t *slab = find_slab_for_object(cache, ptr);
		if (slab)
		{
			old_size = cache->object_size;
			break;
		}
		cache = cache->next;
	}

	// Перевіряємо стандартні кеші якщо не знайшли
	if (old_size == 0)
	{
		for (size_t i = 0; i < g_standard_count; i++)
		{
			if (g_standard_caches[i])
			{
				slab_t *slab = find_slab_for_object(g_standard_caches[i], ptr);
				if (slab)
				{
					old_size = g_standard_caches[i]->object_size;
					break;
				}
			}
		}
	}

	if (old_size == 0)
	{
		return NULL; // Не знайшли об'єкт
	}

	// Якщо новий розмір вміщається в старий кеш, повертаємо той самий вказівник
	if (new_size <= old_size)
	{
		return ptr;
	}

	// Інакше виділяємо новий об'єкт і копіюємо дані
	void *new_ptr = slab_alloc(new_size);
	if (new_ptr)
	{
		memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
		slab_free(ptr);
	}

	return new_ptr;
}

// ============================================================================
// Statistics and Debugging
// ============================================================================

slab_info_t *slab_get_info(void)
{
	// Оновлюємо статистику
	g_slab_info.wasted_memory = g_slab_info.total_memory - g_slab_info.used_memory;

	return &g_slab_info;
}

void slab_cache_info(slab_cache_t *cache)
{
	// Placeholder - requires printk to be implemented properly
	(void)cache;
}

void slab_print_caches(void)
{
	// Placeholder - requires printk to be implemented properly
}

bool slab_validate(void)
{
	// Перевірка консистентності структур
	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		// Перевіряємо кожен slab
		slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

		for (int i = 0; i < 3; i++)
		{
			slab_t *slab = lists[i];
			while (slab)
			{
				// Перевірка capacity
				if (slab->capacity != cache->objects_per_slab)
				{
					return false;
				}

				// Перевірка in_use
				if (slab->in_use > slab->capacity)
				{
					return false;
				}

				slab = slab->next;
			}
		}

		cache = cache->next;
	}

	return true;
}
