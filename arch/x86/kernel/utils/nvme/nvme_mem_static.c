/* nvme_mem_static.c — дуже простий пул сторінок */
#include <stdint.h>
#include <stddef.h>

#include "utils/nvme/nvme.h"

#define PAGE_SIZE 4096
#define NVME_POOL_PAGES 8

static uint8_t nvme_page_pool[NVME_POOL_PAGES * PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static int nvme_pool_next = 0;

/* повертає NULL коли пул закінчився */
void *alloc_page_aligned(void)
{
	if (nvme_pool_next >= NVME_POOL_PAGES)
		return NULL;
	void *p = &nvme_page_pool[nvme_pool_next * PAGE_SIZE];
	nvme_pool_next++;
	return p;
}

/* якщо потрібно n сторінок */
void *alloc_pages_aligned(size_t n)
{
	if (nvme_pool_next + n > NVME_POOL_PAGES)
		return NULL;
	void *p = &nvme_page_pool[nvme_pool_next * PAGE_SIZE];
	nvme_pool_next += n;
	return p;
}
