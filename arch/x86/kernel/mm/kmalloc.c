#include "kmalloc.h"
#include "slab.h"
#include "pmm.h"
#include "higher_half.h"
#include <hubble/string.h>
#include <smp/spinlock.h>
#include <hubble/printk.h>

typedef struct big_alloc_header
{
	size_t pages;
} big_alloc_header_t;

static spinlock_t kmalloc_lock = SPINLOCK_INIT("kmalloc");
static inline uint64_t kmalloc_acquire(void)
{
	uint64_t flags;
	asm volatile("pushfq; pop %0; cli" : "=r"(flags)::"memory");
	spinlock_acquire(&kmalloc_lock);
	return flags;
}

static inline void kmalloc_release(uint64_t flags)
{
	spinlock_release(&kmalloc_lock);
	if (flags & (1ULL << 9))
		asm volatile("sti" ::: "memory");
}

void *kmalloc(size_t size, kmalloc_flags_t flags)
{
	if (size == 0)
	{
		printk(KERN_INFO "KMALLOC: size is 0\n");
		return NULL;
	}
	uint64_t flags_l = kmalloc_acquire();

	if (size <= SLAB_MAX_SIZE)
	{

		void *ptr = (flags & GFP_ZERO) ? slab_calloc(size) : slab_alloc(size);
		kmalloc_release(flags_l);
		if (!ptr)
		{
			printk(KERN_ERR "KMALLOC: failed to allocate %zu bytes\n", size);
		}

		return ptr;
	}

	size_t pages = (size + sizeof(big_alloc_header_t) + PAGE_SIZE - 1) / PAGE_SIZE;
	uint64_t phys = pmm_alloc_pages(pages);
	if (!phys)
	{
		kmalloc_release(flags_l);
		printk(KERN_ERR "KMALLOC: failed to allocate %zu bytes\n", size);
		return NULL;
	}

	big_alloc_header_t *hdr = (big_alloc_header_t *)phys_to_virt(phys);
	hdr->pages = pages;
	void *user_ptr = (void *)(hdr + 1);

	if (flags & GFP_ZERO)
		memset(user_ptr, 0, pages * PAGE_SIZE - sizeof(big_alloc_header_t));

	kmalloc_release(flags_l);
	return user_ptr;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	uint64_t flags = kmalloc_acquire();
	slab_cache_t *cache = find_cache_for_ptr(ptr);
	if (cache)
	{
		slab_free(ptr);
		kmalloc_release(flags);
		return;
	}

	big_alloc_header_t *hdr = (big_alloc_header_t *)ptr - 1;
	if (hdr->pages > 0)
	{
		pmm_free_pages(virt_to_phys((uint64_t)hdr), hdr->pages);
	}
	else
	{
		printk(KERN_WARNING "kfree: pointer %p not recognized\n", ptr);
	}

	kmalloc_release(flags);
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

	uint64_t flags_l = kmalloc_acquire();
	if (old_size >= new_size)
	{
		kmalloc_release(flags_l);
		return ptr;
	}

	kmalloc_release(flags_l);
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
	uint64_t flags = kmalloc_acquire();
	slab_cache_t *cache = find_cache_for_ptr(ptr);
	if (cache)
	{
		kmalloc_release(flags);
		return cache->object_size;
	}

	big_alloc_header_t *hdr = (big_alloc_header_t *)ptr - 1;
	if (hdr->pages > 0)
	{

		kmalloc_release(flags);
		return hdr->pages * PAGE_SIZE - sizeof(big_alloc_header_t);
	}

	kmalloc_release(flags);
	return 0;
}
