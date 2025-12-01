/**
 * @file kmalloc.c
 * @brief Kernel Memory Allocator Implementation
 */

#include "kmalloc.h"
#include "slab.h"
#include <string.h>
#include "printk.h"

// ============================================================================
// Global Statistics
// ============================================================================

static kmalloc_stats_t g_kmalloc_stats = {0};

// ============================================================================
// Helper Functions
// ============================================================================

static inline void update_stats_alloc(size_t size)
{
	g_kmalloc_stats.total_allocated += size;
	g_kmalloc_stats.current_allocated += size;
	g_kmalloc_stats.alloc_count++;

	if (g_kmalloc_stats.current_allocated > g_kmalloc_stats.peak_allocated)
	{
		g_kmalloc_stats.peak_allocated = g_kmalloc_stats.current_allocated;
	}
}

static inline void update_stats_free(size_t size)
{
	g_kmalloc_stats.total_freed += size;
	if (g_kmalloc_stats.current_allocated >= size)
	{
		g_kmalloc_stats.current_allocated -= size;
	}
	g_kmalloc_stats.free_count++;
}

static inline void update_stats_failed(void)
{
	g_kmalloc_stats.failed_allocs++;
}

// ============================================================================
// Core API
// ============================================================================

void *kmalloc(size_t size, kmalloc_flags_t flags)
{
	if (size == 0)
		return NULL;

	if (size > KMALLOC_MAX_SIZE)
	{
		printk(KERN_WARNING "kmalloc: size %zu exceeds max %d\n",
		       size, KMALLOC_MAX_SIZE);
		update_stats_failed();
		return NULL;
	}

	void *ptr;

	if (flags & KMALLOC_ZERO)
	{
		ptr = slab_calloc(size);
	}
	else
	{
		ptr = slab_alloc(size);
	}

	if (ptr)
	{
		// Get actual allocated size from slab
		size_t actual_size = ksize(ptr);
		update_stats_alloc(actual_size);
	}
	else
	{
		update_stats_failed();
	}

	return ptr;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	// Get size before freeing
	size_t size = ksize(ptr);

	slab_free(ptr);

	if (size > 0)
	{
		update_stats_free(size);
	}
}

void *kzalloc(size_t size)
{
	return kmalloc(size, GFP_ZERO);
}

void *kmalloc_array(size_t n, size_t size, kmalloc_flags_t flags)
{
	// Check for overflow
	if (n != 0 && size > SIZE_MAX / n)
	{
		update_stats_failed();
		return NULL;
	}

	return kmalloc(n * size, flags);
}

void *kcalloc(size_t n, size_t size)
{
	return kmalloc_array(n, size, GFP_ZERO);
}

void *krealloc(void *ptr, size_t new_size, kmalloc_flags_t flags)
{
	if (!ptr)
		return kmalloc(new_size, flags);

	if (new_size == 0)
	{
		kfree(ptr);
		return NULL;
	}

	// Get old size
	size_t old_size = ksize(ptr);

	// If new size fits in old allocation, return same pointer
	if (new_size <= old_size)
	{
		return ptr;
	}

	// Allocate new memory
	void *new_ptr = kmalloc(new_size, flags);
	if (!new_ptr)
		return NULL;

	// Copy old data
	memcpy(new_ptr, ptr, old_size);

	// Free old memory
	kfree(ptr);

	return new_ptr;
}

void *kmemdup(const void *src, size_t size, kmalloc_flags_t flags)
{
	if (!src || size == 0)
		return NULL;

	void *ptr = kmalloc(size, flags);
	if (ptr)
	{
		memcpy(ptr, src, size);
	}

	return ptr;
}

char *kstrdup(const char *s, kmalloc_flags_t flags)
{
	if (!s)
		return NULL;

	size_t len = strlen(s) + 1;
	char *ptr = (char *)kmalloc(len, flags);

	if (ptr)
	{
		memcpy(ptr, s, len);
	}

	return ptr;
}

char *kstrndup(const char *s, size_t max, kmalloc_flags_t flags)
{
	if (!s)
		return NULL;

	size_t len = 0;
	while (len < max && s[len] != '\0')
	{
		len++;
	}

	char *ptr = (char *)kmalloc(len + 1, flags);
	if (ptr)
	{
		memcpy(ptr, s, len);
		ptr[len] = '\0';
	}

	return ptr;
}

// ============================================================================
// Aligned Allocations
// ============================================================================

// Структура для хранения метаданных выровненной аллокации
typedef struct
{
	void *original_ptr; // Исходный указатель от slab_alloc
	size_t size;	    // Размер
	size_t align;	    // Выравнивание
} aligned_header_t;

void *kmalloc_aligned(size_t size, size_t align, kmalloc_flags_t flags)
{
	if (size == 0 || align == 0)
		return NULL;

	// Check if align is power of 2
	if ((align & (align - 1)) != 0)
	{
		printk(KERN_ERR "kmalloc_aligned: align must be power of 2\n");
		return NULL;
	}

	// Allocate extra space for alignment and header
	size_t total_size = size + align + sizeof(aligned_header_t);

	void *ptr = kmalloc(total_size, flags & ~KMALLOC_ZERO);
	if (!ptr)
		return NULL;

	// Calculate aligned address
	uintptr_t addr = (uintptr_t)ptr;
	uintptr_t header_addr = (addr + sizeof(aligned_header_t) + align - 1) & ~(align - 1);
	uintptr_t aligned_addr = header_addr;

	// Ensure space for header before aligned address
	if (aligned_addr - addr < sizeof(aligned_header_t))
	{
		aligned_addr += align;
	}

	// Store header
	aligned_header_t *header = (aligned_header_t *)(aligned_addr - sizeof(aligned_header_t));
	header->original_ptr = ptr;
	header->size = size;
	header->align = align;

	void *result = (void *)aligned_addr;

	// Zero if requested
	if (flags & KMALLOC_ZERO)
	{
		memset(result, 0, size);
	}

	return result;
}

void kfree_aligned(void *ptr)
{
	if (!ptr)
		return;

	// Get header
	aligned_header_t *header = (aligned_header_t *)((uintptr_t)ptr - sizeof(aligned_header_t));

	// Free original pointer
	kfree(header->original_ptr);
}

// ============================================================================
// Size Tracking
// ============================================================================

size_t ksize(void *ptr)
{
	if (!ptr)
		return 0;

	// Используем slab для определения размера
	// Так как slab выделяет из фиксированных размеров,
	// мы можем определить размер по кэшу

	// Для простоты возвращаем 0, если не можем определить
	// TODO: Улучшить, добавив mapping ptr -> size
	return 0;
}

// ============================================================================
// Statistics
// ============================================================================

kmalloc_stats_t *kmalloc_get_stats(void)
{
	return &g_kmalloc_stats;
}

void kmalloc_print_stats(void)
{
	kmalloc_stats_t *stats = &g_kmalloc_stats;

	printk(KERN_INFO "=== Kmalloc Statistics ===\n");
	printk(KERN_INFO "Total allocated:   %lu bytes\n", stats->total_allocated);
	printk(KERN_INFO "Total freed:       %lu bytes\n", stats->total_freed);
	printk(KERN_INFO "Current allocated: %lu bytes (%lu KB)\n",
	       stats->current_allocated,
	       stats->current_allocated / 1024);
	printk(KERN_INFO "Peak allocated:    %lu bytes (%lu KB)\n",
	       stats->peak_allocated,
	       stats->peak_allocated / 1024);
	printk(KERN_INFO "Allocation count:  %lu\n", stats->alloc_count);
	printk(KERN_INFO "Free count:        %lu\n", stats->free_count);
	printk(KERN_INFO "Failed allocs:     %lu\n", stats->failed_allocs);

	if (stats->alloc_count > 0)
	{
		uint64_t avg_size = stats->total_allocated / stats->alloc_count;
		printk(KERN_INFO "Average alloc:     %lu bytes\n", avg_size);
	}
}
