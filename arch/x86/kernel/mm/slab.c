#include "slab.h"
#include "pmm.h"
#include "higher_half.h"
#include <hubble/string.h>
#include <hubble/printk.h>

// ============================================================================
// Global State
// ============================================================================

static slab_cache_t *g_cache_list = NULL;
static slab_info_t g_slab_info = {0};

static const size_t g_standard_sizes[8] = {8, 16, 32, 64, 128, 256, 512, 1024};
static const size_t g_standard_count = sizeof(g_standard_sizes) / sizeof(size_t);
static slab_cache_t *g_standard_caches[8] = {NULL};

// ============================================================================
// Helpers
// ============================================================================

void *slab_alloc_page(void)
{
	uint64_t phys = pmm_alloc_page();
	return phys ? (void *)PHYS_TO_VIRT(phys) : NULL;
}

void slab_free_page(void *addr)
{
	if (addr)
		pmm_free_page(VIRT_TO_PHYS(addr));
}

static inline size_t align_up(size_t size, size_t align)
{
	return (size + align - 1) & ~(align - 1);
}

static inline bool is_power_of_2(size_t n) { return n && ((n & (n - 1)) == 0); }

static inline bool is_valid_kernel_ptr(void *ptr)
{
	uint64_t addr = (uint64_t)ptr;
	return IS_KERNEL_VIRT(addr) && addr != 0;
}

static inline bool ptr_in_slab_range(void *ptr, slab_t *slab)
{
	uint64_t start = (uint64_t)slab;
	return ((uint64_t)ptr >= start && (uint64_t)ptr < start + PAGE_SIZE);
}

// ============================================================================
// Cache & Slab Search
// ============================================================================

static slab_cache_t *find_cache(size_t size)
{
	for (size_t i = 0; i < g_standard_count; i++)
		if (size <= g_standard_sizes[i])
			return g_standard_caches[i];

	slab_cache_t *cache = g_cache_list;
	while (cache)
	{
		if (cache->object_size >= size)
			return cache;
		cache = cache->next;
	}
	return NULL;
}

static slab_t *find_slab_for_object(slab_cache_t *cache, void *ptr)
{
	slab_t *slab_list[] = {cache->slabs_partial, cache->slabs_full};
	for (int i = 0; i < 2; i++)
	{
		slab_t *slab = slab_list[i];
		int safety = 0;
		while (slab && safety++ < 100)
		{
			if (!is_valid_kernel_ptr(slab))
				break;
			if (ptr_in_slab_range(ptr, slab))
				return slab;
			slab = slab->next;
		}
	}
	return NULL;
}

slab_cache_t *find_cache_for_ptr(void *ptr)
{
	if (!ptr || !is_valid_kernel_ptr(ptr))
		return NULL;

	for (size_t i = 0; i < g_standard_count; i++)
	{
		slab_cache_t *cache = g_standard_caches[i];
		if (cache && find_slab_for_object(cache, ptr))
			return cache;
	}

	slab_cache_t *cache = g_cache_list;
	int safety = 0;
	while (cache && safety++ < 100)
	{
		if (!is_valid_kernel_ptr(cache))
			break;
		if (find_slab_for_object(cache, ptr))
			return cache;
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
	memset(page, 0, PAGE_SIZE);

	slab_t *slab = (slab_t *)page;
	size_t metadata_size = align_up(sizeof(slab_t), cache->align);

	slab->next = slab->prev = NULL;
	slab->in_use = 0;
	slab->capacity = cache->objects_per_slab;
	slab->start = (uint8_t *)page + metadata_size;

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
		slab->prev->next = slab->next;
	if (slab->next)
		slab->next->prev = slab->prev;

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
		(*list)->prev = slab;
	*list = slab;
}

static void slab_remove_from_list(slab_t **list, slab_t *slab)
{
	if (slab->prev)
		slab->prev->next = slab->next;
	else
		*list = slab->next;
	if (slab->next)
		slab->next->prev = slab->prev;
	slab->next = slab->prev = NULL;
}

// ============================================================================
// Cache API
// ============================================================================

slab_cache_t *slab_cache_create(size_t size, size_t align)
{
	if (size == 0 || size > SLAB_MAX_SIZE)
		return NULL;
	if (!align || !is_power_of_2(align))
		align = 8;

	slab_cache_t *cache = (slab_cache_t *)slab_alloc_page();
	if (!cache)
		return NULL;
	memset(cache, 0, PAGE_SIZE);

	cache->object_size = align_up(size, align);
	cache->align = align;

	size_t metadata_size = align_up(sizeof(slab_t), align);
	size_t available = PAGE_SIZE - metadata_size;
	cache->objects_per_slab = available / cache->object_size;
	if (cache->objects_per_slab == 0)
	{
		slab_free_page(cache);
		return NULL;
	}

	cache->next = g_cache_list;
	g_cache_list = cache;
	g_slab_info.cache_count++;

	return cache;
}

static slab_t *select_slab_for_alloc(slab_cache_t *cache)
{
	if (cache->slabs_partial)
		return cache->slabs_partial;

	if (cache->slabs_free)
	{
		slab_t *slab = cache->slabs_free;
		slab_remove_from_list(&cache->slabs_free, slab);
		slab_add_to_list(&cache->slabs_partial, slab);
		return slab;
	}

	slab_t *slab = slab_create(cache);
	if (slab)
		slab_add_to_list(&cache->slabs_partial, slab);
	return slab;
}

void *slab_cache_alloc(slab_cache_t *cache)
{
	if (!cache || !is_valid_kernel_ptr(cache))
		return NULL;

	slab_t *slab = select_slab_for_alloc(cache);
	if (!slab || !slab->free_list)
		return NULL;

	void *obj = slab->free_list;
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
	slab_t *slab = find_slab_for_object(cache, ptr);
	if (!slab)
		return;

	bool was_full = (slab->in_use == slab->capacity);
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

void slab_free(void *ptr)
{
	slab_cache_t *cache = find_cache_for_ptr(ptr);
	if (!cache)
	{
		printk(KERN_WARNING "slab_free: pointer %p not found\n", ptr);
		return;
	}
	slab_cache_free(cache, ptr);
}

void *slab_alloc(size_t size)
{
	slab_cache_t *cache = find_cache(size);
	return cache ? slab_cache_alloc(cache) : NULL;
}

void *slab_calloc(size_t size)
{
	void *ptr = slab_alloc(size);
	if (ptr)
		memset(ptr, 0, size);
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

	slab_cache_t *cache = find_cache_for_ptr(ptr);
	if (!cache)
		return NULL;

	if (new_size <= cache->object_size)
		return ptr;

	void *new_ptr = slab_alloc(new_size);
	if (new_ptr)
	{
		memcpy(new_ptr, ptr, cache->object_size);
		slab_free(ptr);
	}
	return new_ptr;
}

void slab_init(void)
{
	for (size_t i = 0; i < g_standard_count; i++)
		g_standard_caches[i] = slab_cache_create(g_standard_sizes[i], 8);
}
