#pragma once

#include <stdint.h>
#include <stddef.h>

void pmm_init(uint64_t pmm_start, uint64_t pmm_size);
void *pmm_alloc(size_t size);
void pmm_free(void *addr, size_t pages);
