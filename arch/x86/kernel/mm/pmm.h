#pragma once

#include <stdint.h>
#include <stddef.h>

void pmm_init(uint64_t pmm_start, uint64_t pmm_size);
void *pmm_alloc_pages(size_t size);
void pmm_free_pages(void *addr, size_t size);
