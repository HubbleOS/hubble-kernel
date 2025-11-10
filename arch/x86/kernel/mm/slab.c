#include "slab.h"
#include "pmm.h"
#include <string.h>

// Структура для метаданих об'єкта (використовується коли об'єкт вільний)
typedef struct free_object
{
	struct free_object *next;
} free_object_t;

// Заголовок для великих виділень (більше PAGE_SIZE)
typedef struct large_alloc
{
	size_t size;
	uint32_t magic;
} large_alloc_t;

// Глобальні змінні
static slab_cache_t *g_cache_list = NULL; // Список всіх кешів
static bool g_slab_initialized = false;

// Стандартні кеші для різних розмірів
static slab_cache_t *g_cache_8 = NULL;
static slab_cache_t *g_cache_16 = NULL;
static slab_cache_t *g_cache_32 = NULL;
static slab_cache_t *g_cache_64 = NULL;
static slab_cache_t *g_cache_128 = NULL;
static slab_cache_t *g_cache_256 = NULL;
static slab_cache_t *g_cache_512 = NULL;
static slab_cache_t *g_cache_1024 = NULL;
static slab_cache_t *g_cache_2048 = NULL;

// === Helper Functions ===

static inline size_t align_up(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

static inline bool is_power_of_2(size_t x)
{
	return x && !(x & (x - 1));
}

static inline uintptr_t align_up_u(uintptr_t v, uintptr_t a)
{
	return (v + a - 1) & ~(a - 1);
}

// Створює новий slab (виділяє сторінку та ініціалізує об'єкти)
// static slab_t *slab_create(slab_cache_t *cache)
// {
// 	// Виділяємо одну сторінку через PMM
// 	uint64_t page = pmm_alloc_page();
// 	if (!page)
// 	{
// 		return NULL;
// 	}

// 	slab_t *slab = (slab_t *)page;

// 	// Обчислюємо скільки об'єктів вміщується
// 	size_t usable_size = PAGE_SIZE - sizeof(slab_t);
// 	size_t object_count = usable_size / cache->object_size;

// 	slab->next = NULL;
// 	slab->free_count = object_count;
// 	slab->object_count = object_count;

// 	// Ініціалізуємо free list
// 	uint8_t *obj_start = (uint8_t *)slab + sizeof(slab_t);
// 	slab->free_list = NULL;

// 	for (size_t i = 0; i < object_count; i++)
// 	{
// 		free_object_t *obj = (free_object_t *)(obj_start + i * cache->object_size);
// 		obj->next = slab->free_list;
// 		slab->free_list = obj;
// 	}

// 	cache->slab_count++;
// 	cache->total_objects += object_count;

// 	return slab;
// }

static slab_t *slab_create(slab_cache_t *cache)
{
	uint64_t page = pmm_alloc_page();
	if (!page)
		return NULL;

	slab_t *slab = (slab_t *)page;

	// Вычисляем выровненное начало объектов: после заголовка slab_t, выровненное по cache->align
	uintptr_t slab_base = (uintptr_t)slab;
	uintptr_t after_header = slab_base + sizeof(slab_t);
	uintptr_t obj_start_addr = align_up_u(after_header, cache->align);
	uint8_t *obj_start = (uint8_t *)obj_start_addr;

	// usable_size — от obj_start до конца страницы
	size_t usable_size = PAGE_SIZE - (obj_start_addr - slab_base);

	// Сколько объектов помещается
	size_t object_count = usable_size / cache->object_size;
	if (object_count == 0)
	{
		// объект слишком большой для одной страницы
		pmm_free_page((uint64_t)slab);
		return NULL;
	}

	slab->next = NULL;
	slab->free_count = object_count;
	slab->object_count = object_count;
	slab->free_list = NULL;

	for (size_t i = 0; i < object_count; i++)
	{
		free_object_t *obj = (free_object_t *)(obj_start + i * cache->object_size);
		obj->next = slab->free_list;
		slab->free_list = obj;
	}

	cache->slab_count++;
	cache->total_objects += object_count;

	return slab;
}

// Видаляє slab та звільняє сторінку
static void slab_destroy(slab_t *slab)
{
	if (!slab)
		return;
	pmm_free_page((uint64_t)slab);
}

// Переміщує slab між списками
static void slab_move(slab_t **from_list, slab_t **to_list, slab_t *slab)
{
	// Видаляємо з from_list
	slab_t **current = from_list;
	while (*current)
	{
		if (*current == slab)
		{
			*current = slab->next;
			break;
		}
		current = &(*current)->next;
	}

	// Додаємо до to_list
	slab->next = *to_list;
	*to_list = slab;
}

// Знаходить slab, якому належить об'єкт
static slab_t *slab_find(slab_cache_t *cache, void *ptr)
{
	uint64_t addr = (uint64_t)ptr;
	uint64_t slab_addr = addr & ~(PAGE_SIZE - 1); // Вирівнюємо до початку сторінки

	// Шукаємо у всіх списках
	slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

	for (int i = 0; i < 3; i++)
	{
		slab_t *slab = lists[i];
		while (slab)
		{
			if ((uint64_t)slab == slab_addr)
			{
				return slab;
			}
			slab = slab->next;
		}
	}

	return NULL;
}

// === Public API ===

void slab_init(void)
{
	if (g_slab_initialized)
	{
		return;
	}

	// Створюємо стандартні кеші
	g_cache_8 = slab_cache_create("kmalloc-8", SLAB_SIZE_8, 8);
	g_cache_16 = slab_cache_create("kmalloc-16", SLAB_SIZE_16, 16);
	g_cache_32 = slab_cache_create("kmalloc-32", SLAB_SIZE_32, 32);
	g_cache_64 = slab_cache_create("kmalloc-64", SLAB_SIZE_64, 64);
	g_cache_128 = slab_cache_create("kmalloc-128", SLAB_SIZE_128, 128);
	g_cache_256 = slab_cache_create("kmalloc-256", SLAB_SIZE_256, 256);
	g_cache_512 = slab_cache_create("kmalloc-512", SLAB_SIZE_512, 512);
	g_cache_1024 = slab_cache_create("kmalloc-1024", SLAB_SIZE_1024, 1024);
	g_cache_2048 = slab_cache_create("kmalloc-2048", SLAB_SIZE_2048, 2048);

	g_slab_initialized = true;
}

slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align)
{
	// Мінімальне вирівнювання - 8 байт
	if (align == 0)
	{
		align = 8;
	}

	if (!is_power_of_2(align))
	{
		return NULL;
	}

	// Вирівнюємо розмір об'єкта
	size_t aligned_size = align_up(size, align);

	// Мінімальний розмір - розмір вказівника для free list
	if (aligned_size < sizeof(free_object_t))
	{
		aligned_size = sizeof(free_object_t);
	}

	// Виділяємо структуру cache через PMM
	uint64_t cache_page = pmm_alloc_page();
	if (!cache_page)
	{
		return NULL;
	}

	slab_cache_t *cache = (slab_cache_t *)cache_page;
	cache->name = name;
	cache->object_size = aligned_size;
	cache->align = align;
	cache->slabs_full = NULL;
	cache->slabs_partial = NULL;
	cache->slabs_free = NULL;
	cache->total_objects = 0;
	cache->used_objects = 0;
	cache->slab_count = 0;

	// Додаємо до глобального списку
	cache->next = g_cache_list;
	g_cache_list = cache;

	return cache;
}

void slab_cache_destroy(slab_cache_t *cache)
{
	if (!cache)
		return;

	// Звільняємо всі slab'и
	slab_t *lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_free};

	for (int i = 0; i < 3; i++)
	{
		slab_t *slab = lists[i];
		while (slab)
		{
			slab_t *next = slab->next;
			slab_destroy(slab);
			slab = next;
		}
	}

	// Видаляємо з глобального списку
	slab_cache_t **current = &g_cache_list;
	while (*current)
	{
		if (*current == cache)
		{
			*current = cache->next;
			break;
		}
		current = &(*current)->next;
	}

	// Звільняємо структуру cache
	pmm_free_page((uint64_t)cache);
}

void *slab_alloc(slab_cache_t *cache)
{
	if (!cache)
		return NULL;

	slab_t *slab = NULL;

	// Спочатку шукаємо частково заповнений slab
	if (cache->slabs_partial)
	{
		slab = cache->slabs_partial;
	}
	// Потім шукаємо вільний slab
	else if (cache->slabs_free)
	{
		slab = cache->slabs_free;
	}
	// Якщо немає - створюємо новий
	else
	{
		slab = slab_create(cache);
		if (!slab)
		{
			return NULL;
		}
		slab->next = cache->slabs_free;
		cache->slabs_free = slab;
	}

	// Виділяємо об'єкт
	free_object_t *obj = (free_object_t *)slab->free_list;
	if (!obj)
	{
		return NULL; // Не повинно статися
	}

	slab->free_list = obj->next;
	slab->free_count--;
	cache->used_objects++;

	// Переміщуємо slab між списками якщо потрібно
	if (slab->free_count == 0)
	{
		// Slab став повним
		if (slab == cache->slabs_partial)
		{
			cache->slabs_partial = slab->next;
			slab->next = cache->slabs_full;
			cache->slabs_full = slab;
		}
		else if (slab == cache->slabs_free)
		{
			cache->slabs_free = slab->next;
			slab->next = cache->slabs_full;
			cache->slabs_full = slab;
		}
	}
	else if (slab->free_count < slab->object_count)
	{
		// Slab став частково заповненим
		if (slab == cache->slabs_free)
		{
			cache->slabs_free = slab->next;
			slab->next = cache->slabs_partial;
			cache->slabs_partial = slab;
		}
	}

	return obj;
}

void slab_free(slab_cache_t *cache, void *ptr)
{
	if (!cache || !ptr)
		return;

	// Знаходимо slab
	slab_t *slab = slab_find(cache, ptr);
	if (!slab)
	{
		return; // Помилка: об'єкт не належить цьому cache
	}

	bool was_full = (slab->free_count == 0);

	// Повертаємо об'єкт у free list
	free_object_t *obj = (free_object_t *)ptr;
	obj->next = slab->free_list;
	slab->free_list = obj;
	slab->free_count++;
	cache->used_objects--;

	// Переміщуємо slab між списками
	if (slab->free_count == slab->object_count)
	{
		// Slab став повністю вільним
		if (was_full)
		{
			slab_move(&cache->slabs_full, &cache->slabs_free, slab);
		}
		else
		{
			slab_move(&cache->slabs_partial, &cache->slabs_free, slab);
		}
	}
	else if (was_full)
	{
		// Slab був повним, став частково заповненим
		slab_move(&cache->slabs_full, &cache->slabs_partial, slab);
	}
}

slab_cache_t *slab_get_cache(size_t size)
{
	if (size <= 8)
		return g_cache_8;
	if (size <= 16)
		return g_cache_16;
	if (size <= 32)
		return g_cache_32;
	if (size <= 64)
		return g_cache_64;
	if (size <= 128)
		return g_cache_128;
	if (size <= 256)
		return g_cache_256;
	if (size <= 512)
		return g_cache_512;
	if (size <= 1024)
		return g_cache_1024;
	if (size <= 2048)
		return g_cache_2048;
	return NULL;
}

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	// Для малих виділень використовуємо slab
	if (size <= 2048)
	{
		slab_cache_t *cache = slab_get_cache(size);
		if (cache)
		{
			return slab_alloc(cache);
		}
	}

	// Для великих виділень використовуємо PMM напряму
	size_t total_size = size + sizeof(large_alloc_t);
	size_t pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

	uint64_t addr = pmm_alloc_pages(pages);
	if (!addr)
	{
		return NULL;
	}

	large_alloc_t *header = (large_alloc_t *)addr;
	header->size = pages * PAGE_SIZE;
	header->magic = SLAB_MAGIC;

	return (void *)(addr + sizeof(large_alloc_t));
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	// Перевіряємо чи це велике виділення
	large_alloc_t *header = (large_alloc_t *)((uint64_t)ptr - sizeof(large_alloc_t));

	if (header->magic == SLAB_MAGIC)
	{
		// Це велике виділення
		size_t pages = header->size / PAGE_SIZE;
		pmm_free_pages((uint64_t)header, pages);
		return;
	}

	// Це slab виділення - шукаємо відповідний cache
	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		slab_t *slab = slab_find(cache, ptr);
		if (slab)
		{
			slab_free(cache, ptr);
			return;
		}
		cache = cache->next;
	}
}

// void *kmalloc_aligned(size_t size, size_t align)
// {
// 	if (!is_power_of_2(align))
// 	{
// 		return NULL;
// 	}

// 	// Для невеликих розмірів шукаємо cache з потрібним вирівнюванням
// 	if (size <= 2048 && align <= 2048)
// 	{
// 		slab_cache_t *cache = g_cache_list;
// 		while (cache)
// 		{
// 			if (cache->object_size >= size && cache->align >= align)
// 			{
// 				return slab_alloc(cache);
// 			}
// 			cache = cache->next;
// 		}
// 	}

// 	// Інакше використовуємо PMM
// 	return kmalloc(size);
// }

void *kmalloc_aligned(size_t size, size_t align)
{
	if (!is_power_of_2(align))
		return NULL;

	// Попробуем найти slab-кеш
	if (size <= 2048 && align <= 2048)
	{
		slab_cache_t *cache = g_cache_list;
		while (cache)
		{
			if (cache->object_size >= size && cache->align >= align)
			{
				return slab_alloc(cache);
			}
			cache = cache->next;
		}
	}

	// Фоллбек: выделяем через PMM несколько страниц и возвращаем выровненный адрес.
	// Нужно выделить дополнительно (align / PAGE_SIZE + 1) страниц, чтобы получить выровненный блок внутри.
	size_t total_size = size + sizeof(large_alloc_t) + align;
	size_t pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

	uint64_t raw = pmm_alloc_pages(pages);
	if (!raw)
		return NULL;

	uintptr_t raw_addr = (uintptr_t)raw + sizeof(large_alloc_t);
	uintptr_t aligned = (raw_addr + (align - 1)) & ~(align - 1);

	// Сохраняем header непосредственно перед aligned-адресом
	large_alloc_t *header = (large_alloc_t *)(aligned - sizeof(large_alloc_t));
	header->size = pages * PAGE_SIZE;
	header->magic = SLAB_MAGIC;

	return (void *)aligned;
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

	// Определяем, большой ли блок
	large_alloc_t *header = (large_alloc_t *)((uintptr_t)ptr - sizeof(large_alloc_t));
	if (header->magic == SLAB_MAGIC)
	{
		// Это большое выделение через PMM
		size_t old_size = header->size - sizeof(large_alloc_t);
		if (new_size <= old_size)
			return ptr; // помещается в старый блок

		void *new_ptr = kmalloc(new_size);
		if (!new_ptr)
			return NULL;

		memcpy(new_ptr, ptr, old_size);
		kfree(ptr);
		return new_ptr;
	}

	// Иначе это slab-выделение
	slab_cache_t *cache = g_cache_list;
	size_t old_size = 0;

	while (cache)
	{
		slab_t *slab = slab_find(cache, ptr);
		if (slab)
		{
			old_size = cache->object_size;
			break;
		}
		cache = cache->next;
	}

	if (!old_size)
		return NULL; // не нашли блок

	if (new_size <= old_size)
		return ptr; // помещается в старый блок

	void *new_ptr = kmalloc(new_size);
	if (!new_ptr)
		return NULL;

	memcpy(new_ptr, ptr, old_size);
	kfree(ptr);
	return new_ptr;
}
