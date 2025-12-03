#include "kmalloc.h"
#include "slab.h"
#include <string.h>
#include "printk.h"

void *kmalloc(size_t size, kmalloc_flags_t flags)
{
	if (size == 0 || size > SLAB_MAX_SIZE)
		return NULL;

	void *ptr = (flags & KMALLOC_ZERO) ? slab_calloc(size) : slab_alloc(size);

	return ptr;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;
	slab_free(ptr);
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
	if (new_size <= old_size)
		return ptr;

	void *new_ptr = kmalloc(new_size, flags);
	if (new_ptr)
	{
		memcpy(new_ptr, ptr, old_size);
		kfree(ptr);
	}
	return new_ptr;
}

void *kmemdup(const void *src, size_t size, kmalloc_flags_t flags)
{
	if (!src || size == 0)
		return NULL;
	void *ptr = kmalloc(size, flags);
	if (ptr)
		memcpy(ptr, src, size);
	return ptr;
}

char *kstrdup(const char *s, kmalloc_flags_t flags)
{
	if (!s)
		return NULL;
	size_t len = strlen(s) + 1;
	char *ptr = kmalloc(len, flags);
	if (ptr)
		memcpy(ptr, s, len);
	return ptr;
}

char *kstrndup(const char *s, size_t max, kmalloc_flags_t flags)
{
	if (!s)
		return NULL;
	size_t len = 0;
	while (len < max && s[len])
		len++;
	char *ptr = kmalloc(len + 1, flags);
	if (ptr)
	{
		memcpy(ptr, s, len);
		ptr[len] = '\0';
	}
	return ptr;
}

size_t ksize(void *ptr)
{
	slab_cache_t *cache = find_cache_for_ptr(ptr);
	return cache ? cache->object_size : 0;
}
