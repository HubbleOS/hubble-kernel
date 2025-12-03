#include "kmalloc.h"
#include "slab.h"
#include "pmm.h"
#include "higher_half.h"
#include <string.h>
#include "printk.h"

typedef struct big_alloc_header
{
	size_t pages;
} big_alloc_header_t;

void *kmalloc(size_t size, kmalloc_flags_t flags)
{
	if (size == 0)
		return NULL;

	if (size <= SLAB_MAX_SIZE)
	{
		return (flags & GFP_ZERO) ? slab_calloc(size) : slab_alloc(size);
	}

	size_t pages = (size + sizeof(big_alloc_header_t) + PAGE_SIZE - 1) / PAGE_SIZE;
	uint64_t phys = pmm_alloc_pages(pages);
	if (!phys)
		return NULL;

	big_alloc_header_t *hdr = (big_alloc_header_t *)PHYS_TO_VIRT(phys);
	hdr->pages = pages;
	void *user_ptr = (void *)(hdr + 1);

	if (flags & GFP_ZERO)
		memset(user_ptr, 0, pages * PAGE_SIZE - sizeof(big_alloc_header_t));

	return user_ptr;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	slab_cache_t *cache = find_cache_for_ptr(ptr);
	if (cache)
	{
		slab_free(ptr);
		return;
	}

	big_alloc_header_t *hdr = (big_alloc_header_t *)ptr - 1;
	if (hdr->pages > 0)
	{
		pmm_free_pages(VIRT_TO_PHYS(hdr), hdr->pages);
	}
	else
	{
		printk(KERN_WARNING "kfree: pointer %p not recognized\n", ptr);
	}
}

void *kzalloc(size_t size) { return kmalloc(size, GFP_ZERO); }

void *kmalloc_array(size_t n, size_t size, kmalloc_flags_t flags)
{
	if (n != 0 && size > SIZE_MAX / n)
		return NULL;
	return kmalloc(n * size, flags);
}

void *kcalloc(size_t n, size_t size) { return kmalloc_array(n, size, GFP_ZERO); }

void *krealloc(void *ptr, size_t new_size, kmalloc_flags_t flags)
{
	if (!ptr)
		return kmalloc(new_size, flags);
	if (new_size == 0)
	{
		kfree(ptr);
		return NULL;
	}

	size_t old_size = ksize(ptr);
	if (old_size >= new_size)
		return ptr;

	void *new_ptr = kmalloc(new_size, flags);
	if (new_ptr)
	{
		memcpy(new_ptr, ptr, old_size);
		kfree(ptr);
	}
	return new_ptr;
}

size_t ksize(void *ptr)
{
	slab_cache_t *cache = find_cache_for_ptr(ptr);
	if (cache)
		return cache->object_size;

	big_alloc_header_t *hdr = (big_alloc_header_t *)ptr - 1;
	if (hdr->pages > 0)
		return hdr->pages * PAGE_SIZE - sizeof(big_alloc_header_t);

	return 0;
}
