/**
 * @file kmalloc.c
 * @brief Kernel Memory Allocator - FIXED VERSION
 */

#include <mm/kmalloc.h>
#include <mm/slab.h>
#include <mm/vmm.h>
#include <string.h>
#include "printk.h"

// ============================================================================
// Configuration
// ============================================================================

#define PAGE_SIZE 4096
#define ALLOC_MAGIC 0xC0FFEEAA
#define SLAB_THRESHOLD 2048 // Використовуємо slab до 2KB, потім VMM

// Заголовок виділення
typedef struct alloc_header
{
	uint32_t magic;	   // Магічне число для валідації
	uint32_t flags;	   // 0 = slab, 1 = vmm
	size_t alloc_size; // Розмір, який було виділено (з заголовком)
	size_t user_size;  // Розмір, який запросив користувач
} alloc_header_t;

// Вирівнювання заголовка
#define HEADER_SIZE sizeof(alloc_header_t)
#define HEADER_ALIGN 16
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Знайти найближчий slab bucket для заданого розміру
 */
static size_t get_slab_bucket(size_t size)
{
	// Slab buckets: 8, 16, 32, 64, 128, 256, 512, 1024, 2048
	if (size <= 8)
		return 8;
	if (size <= 16)
		return 16;
	if (size <= 32)
		return 32;
	if (size <= 64)
		return 64;
	if (size <= 128)
		return 128;
	if (size <= 256)
		return 256;
	if (size <= 512)
		return 512;
	if (size <= 1024)
		return 1024;
	if (size <= 2048)
		return 2048;

	return 0; // Too large for slab
}

/**
 * @brief Перевірка валідності заголовка
 */
static inline bool validate_header(alloc_header_t *hdr)
{
	return hdr && hdr->magic == ALLOC_MAGIC;
}

// ============================================================================
// Main API
// ============================================================================

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	// Вирівнюємо заголовок
	size_t header_size = ALIGN_UP(HEADER_SIZE, HEADER_ALIGN);

	// Загальний розмір, який потрібно виділити
	size_t total_needed = header_size + size;

	alloc_header_t *hdr = NULL;
	size_t allocated = 0;

	// === ВИБІР СТРАТЕГІЇ ВИДІЛЕННЯ ===

	if (total_needed <= SLAB_THRESHOLD)
	{
		// ==========================================
		// ВИКОРИСТОВУЄМО SLAB ALLOCATOR
		// ==========================================

		// Знаходимо найближчий slab bucket
		size_t bucket = get_slab_bucket(total_needed);

		if (bucket == 0)
		{
			// Розмір занадто великий для slab, переходимо на VMM
			goto use_vmm;
		}

		// Виділяємо з slab
		hdr = (alloc_header_t *)slab_alloc(bucket);
		if (!hdr)
		{
			printk(KERN_ERR "kmalloc: slab_alloc(%lu) failed\n", bucket);
			return NULL;
		}

		allocated = bucket;
		hdr->flags = 0; // slab
	}
	else
	{
	use_vmm:
		// ==========================================
		// ВИКОРИСТОВУЄМО VMM (великі об'єкти)
		// ==========================================

		// Обчислюємо кількість сторінок
		size_t pages = (total_needed + PAGE_SIZE - 1) / PAGE_SIZE;

		hdr = (alloc_header_t *)vmm_alloc_kernel_pages(pages);
		if (!hdr)
		{
			printk(KERN_ERR "kmalloc: vmm_alloc_kernel_pages(%lu) failed\n", pages);
			return NULL;
		}

		allocated = pages * PAGE_SIZE;
		hdr->flags = 1; // vmm
	}

	// === ІНІЦІАЛІЗАЦІЯ ЗАГОЛОВКА ===

	hdr->magic = ALLOC_MAGIC;
	hdr->alloc_size = allocated;
	hdr->user_size = size;

	// Повертаємо вказівник ПІСЛЯ заголовка
	void *user_ptr = (void *)((uint8_t *)hdr + header_size);

	return user_ptr;
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	// Отримуємо заголовок
	size_t header_size = ALIGN_UP(HEADER_SIZE, HEADER_ALIGN);
	alloc_header_t *hdr = (alloc_header_t *)((uint8_t *)ptr - header_size);

	// Валідація
	if (!validate_header(hdr))
	{
		printk(KERN_ERR "kfree: Invalid header at %p (magic=0x%x, expected=0x%x)\n",
		       hdr, hdr->magic, ALLOC_MAGIC);
		return;
	}

	// Звільняємо в залежності від типу
	if (hdr->flags == 0)
	{
		// Slab allocation - потрібно звільнити весь bucket
		slab_free(hdr);
	}
	else if (hdr->flags == 1)
	{
		// VMM allocation
		size_t pages = (hdr->alloc_size + PAGE_SIZE - 1) / PAGE_SIZE;
		vmm_free_kernel_pages(hdr, pages);
	}
	else
	{
		printk(KERN_ERR "kfree: Invalid flags %u at %p\n", hdr->flags, hdr);
	}
}

void *kcalloc(size_t size)
{
	void *ptr = kmalloc(size);
	if (ptr)
	{
		memset(ptr, 0, size);
	}
	return ptr;
}

void *krealloc(void *ptr, size_t new_size)
{
	// Якщо ptr == NULL, просто виділяємо нову пам'ять
	if (!ptr)
		return kmalloc(new_size);

	// Якщо new_size == 0, звільняємо пам'ять
	if (new_size == 0)
	{
		kfree(ptr);
		return NULL;
	}

	// Отримуємо заголовок
	size_t header_size = ALIGN_UP(HEADER_SIZE, HEADER_ALIGN);
	alloc_header_t *hdr = (alloc_header_t *)((uint8_t *)ptr - header_size);

	// Валідація
	if (!validate_header(hdr))
	{
		printk(KERN_ERR "krealloc: Invalid header at %p\n", hdr);
		return NULL;
	}

	size_t old_size = hdr->user_size;

	// Оптимізація: якщо новий розмір менший і влізає в той самий bucket/pages
	if (new_size <= old_size)
	{
		size_t total_needed = header_size + new_size;

		// Перевіряємо, чи залишаємось в тому ж bucket/allocation
		if (hdr->flags == 0) // slab
		{
			size_t old_bucket = get_slab_bucket(header_size + old_size);
			size_t new_bucket = get_slab_bucket(total_needed);

			if (new_bucket == old_bucket)
			{
				// Можемо просто оновити розмір
				hdr->user_size = new_size;
				return ptr;
			}
		}
		else // vmm
		{
			size_t old_pages = (hdr->alloc_size + PAGE_SIZE - 1) / PAGE_SIZE;
			size_t new_pages = (total_needed + PAGE_SIZE - 1) / PAGE_SIZE;

			if (new_pages == old_pages)
			{
				// Можемо просто оновити розмір
				hdr->user_size = new_size;
				return ptr;
			}
		}
	}

	// Якщо не можемо оптимізувати, виділяємо новий блок
	void *new_ptr = kmalloc(new_size);
	if (!new_ptr)
		return NULL;

	// Копіюємо дані (мінімум зі старого та нового розміру)
	size_t copy_size = (old_size < new_size) ? old_size : new_size;
	memcpy(new_ptr, ptr, copy_size);

	// Звільняємо старий блок
	kfree(ptr);

	return new_ptr;
}

// ============================================================================
// Debugging and Statistics
// ============================================================================

void kmalloc_stats(void)
{
	printk(KERN_INFO "=== Kernel Memory Allocator Statistics ===\n");

	// Отримуємо статистику від slab
	slab_info_t *slab_info = slab_get_info();
	printk(KERN_INFO "Slab Allocator:\n");
	printk(KERN_INFO "  Total memory: %lu KB\n", slab_info->total_memory / 1024);
	printk(KERN_INFO "  Used memory:  %lu KB\n", slab_info->used_memory / 1024);
	printk(KERN_INFO "  Allocations:  %lu\n", slab_info->total_allocations);
	printk(KERN_INFO "  Frees:        %lu\n", slab_info->total_frees);

	// Отримуємо статистику від VMM
	vmm_info_t *vmm_info = vmm_get_info();
	printk(KERN_INFO "VMM Allocator:\n");
	printk(KERN_INFO "  Kernel pages: %lu (%lu MB)\n",
	       vmm_info->kernel_pages,
	       (vmm_info->kernel_pages * PAGE_SIZE) / (1024 * 1024));
}

/**
 * @brief Тестування kmalloc
 */
void kmalloc_test(void)
{
	printk(KERN_INFO "=== Testing kmalloc ===\n");

	// Test 1: Малий об'єкт (slab)
	char *str1 = (char *)kmalloc(32);
	if (str1)
	{
		memcpy(str1, "Hello from kmalloc!", 20);
		printk(KERN_INFO "✓ Small allocation: %s\n", str1);
		kfree(str1);
	}

	// Test 2: Середній об'єкт (slab)
	int *arr = (int *)kmalloc(100 * sizeof(int));
	if (arr)
	{
		for (int i = 0; i < 100; i++)
			arr[i] = i;

		printk(KERN_INFO "✓ Array allocation: arr[50] = %d\n", arr[50]);
		kfree(arr);
	}

	// Test 3: Великий об'єкт (vmm)
	void *big = kmalloc(8192);
	if (big)
	{
		printk(KERN_INFO "✓ Large allocation: %lu bytes\n", 8192);
		kfree(big);
	}

	// Test 4: kcalloc
	int *zeros = (int *)kcalloc(50 * sizeof(int));
	if (zeros)
	{
		bool all_zero = true;
		for (int i = 0; i < 50; i++)
		{
			if (zeros[i] != 0)
			{
				all_zero = false;
				break;
			}
		}
		printk(KERN_INFO "✓ kcalloc test: %s\n", all_zero ? "passed" : "failed");
		kfree(zeros);
	}

	// Test 5: krealloc
	char *buf = (char *)kmalloc(64);
	if (buf)
	{
		memcpy(buf, "Initial", 8);
		buf = (char *)krealloc(buf, 128);
		if (buf)
		{
			printk(KERN_INFO "✓ krealloc test: %s\n", buf);
			kfree(buf);
		}
	}

	printk(KERN_INFO "=== kmalloc tests complete ===\n");
}
