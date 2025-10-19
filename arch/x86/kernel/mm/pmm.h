#pragma once

#include <stdint.h>
#include <stddef.h>

void pmm_init(uint64_t pmm_start, uint64_t pmm_size);
void *pmm_alloc(size_t pages);
void pmm_free(void *addr, size_t pages);

// Получить физический адрес из того что возвращает pmm_alloc
uint64_t pmm_get_phys(void *virt_ptr);
