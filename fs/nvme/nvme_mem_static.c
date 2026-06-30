/* ── NVMe static memory pool ──────────────────────────────────────
 * Simple page-aligned allocator for NVMe DMA buffers. Uses a
 * small static pool to avoid dynamic allocation during init.
 * ────────────────────────────────────────────────────────────────── */

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define NVME_POOL_PAGES 8

static uint8_t nvme_page_pool[NVME_POOL_PAGES * PAGE_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static int nvme_pool_next = 0;

/** @brief Allocate a single page-aligned buffer from the static pool.
 *
 * @return Pointer to the buffer, or NULL if the pool is exhausted.
 */
void *alloc_page_aligned(void)
{
	if (nvme_pool_next >= NVME_POOL_PAGES)
		return NULL;
	void *p = &nvme_page_pool[nvme_pool_next * PAGE_SIZE];
	nvme_pool_next++;
	return p;
}

/** @brief Allocate n contiguous page-aligned buffers.
 *
 * @param n  Number of pages requested.
 * @return Pointer to the first page, or NULL if insufficient space.
 */
void *alloc_pages_aligned(size_t n)
{
	if (nvme_pool_next + n > NVME_POOL_PAGES)
		return NULL;
	void *p = &nvme_page_pool[nvme_pool_next * PAGE_SIZE];
	nvme_pool_next += n;
	return p;
}
