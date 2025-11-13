#include <mm/kmalloc.h>
#include <string.h>
#include "printk.h"

typedef struct alloc_header
{
	uint32_t magic;
	uint32_t flags; // 0 = slab, 1 = vmm
	size_t size;	// розмір БЕЗ заголовка
} alloc_header_t;

#define PAGE_SIZE 4096
#define ALLOC_MAGIC 0xC0FFEEAA
#define SLAB_MAX_SIZE 4096

// Вирівнювання заголовка на 16 байт
#define HEADER_ALIGN 16
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

void *kmalloc(size_t size)
{
	if (size == 0)
		return NULL;

	alloc_header_t *hdr;
	size_t aligned_header = ALIGN_UP(sizeof(alloc_header_t), HEADER_ALIGN);

	// КРИТИЧНО: total_size = заголовок + дані
	size_t total_size = aligned_header + size;

	// Вибір алокатора на основі TOTAL SIZE
	if (total_size <= SLAB_MAX_SIZE)
	{
		// Виділяємо TOTAL_SIZE зі slab (включаючи заголовок!)
		hdr = slab_alloc(total_size);
		if (!hdr)
			return NULL;

		hdr->flags = 0; // slab
	}
	else
	{
		// Для великих об'єктів використовуємо vmm
		size_t pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

		hdr = vmm_alloc_kernel_pages(pages);
		if (!hdr)
			return NULL;

		hdr->flags = 1; // vmm
	}

	hdr->magic = ALLOC_MAGIC;
	hdr->size = size; // зберігаємо ОРИГІНАЛЬНИЙ розмір (без заголовка)

	// Повертаємо вказівник ПІСЛЯ заголовка
	return (void *)((uint8_t *)hdr + aligned_header);
}

void kfree(void *ptr)
{
	if (!ptr)
		return;

	size_t aligned_header = ALIGN_UP(sizeof(alloc_header_t), HEADER_ALIGN);
	alloc_header_t *hdr = (alloc_header_t *)((uint8_t *)ptr - aligned_header);

	// КРИТИЧНА ВАЛІДАЦІЯ
	if (hdr->magic != ALLOC_MAGIC)
	{
		printk(KERN_ERR "kfree: Invalid magic 0x%x at %p (expected 0x%x)\n",
		       hdr->magic, hdr, ALLOC_MAGIC);
		return;
	}

	if (hdr->flags == 0) // slab
	{
		// Звільняємо весь блок (включаючи заголовок)
		slab_free(hdr);
	}
	else if (hdr->flags == 1) // vmm
	{
		size_t total_size = hdr->size + aligned_header;
		size_t pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
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
		memset(ptr, 0, size);
	return ptr;
}

void *krealloc(void *ptr, size_t new_size)
{
	if (!ptr)
		return kmalloc(new_size);

	size_t aligned_header = ALIGN_UP(sizeof(alloc_header_t), HEADER_ALIGN);
	alloc_header_t *hdr = (alloc_header_t *)((uint8_t *)ptr - aligned_header);

	if (hdr->magic != ALLOC_MAGIC)
	{
		printk(KERN_ERR "krealloc: Invalid pointer %p\n", ptr);
		return NULL;
	}

	size_t old_size = hdr->size;

	// Оптимізація: якщо новий розмір менший, просто оновлюємо метадані
	if (new_size <= old_size)
	{
		hdr->size = new_size;
		return ptr;
	}

	// Перевіряємо, чи можемо залишитись в тому ж slab bucket
	size_t old_total = old_size + aligned_header;
	size_t new_total = new_size + aligned_header;

	// Якщо обидва в одному діапазоні (slab або vmm) і є місце
	if (hdr->flags == 0) // старий блок в slab
	{
		// Перевіряємо, чи новий розмір влізе в той самий slab bucket
		// (Це залежить від реалізації slab, але зазвичай можна оптимізувати)
		// Для простоти - завжди виділяємо новий блок
	}

	// Виділяємо новий блок
	void *new_ptr = kmalloc(new_size);
	if (!new_ptr)
		return NULL;

	// Копіюємо ДАНі (без заголовка)
	memcpy(new_ptr, ptr, old_size);

	kfree(ptr);

	return new_ptr;
}
