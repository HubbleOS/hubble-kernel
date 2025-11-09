// #pragma once

// #include <stdint.h>
// #include <stddef.h>

// void pmm_init(uint64_t pmm_start, uint64_t pmm_size);
// void *pmm_alloc_phys(size_t pages);
// void pmm_free_physvoid *addr, size_t pages);

// // Получить физический адрес из того что возвращает pmm_alloc
// uint64_t pmm_get_phys(void *virt_ptr);

#pragma once
#include <stdint.h>
#include <stddef.h>

// Инициализация PMM (работает через identity mapping)
void pmm_init(uint64_t heap_phys_start, uint64_t heap_size);

// Включение physmap после vmm_init
void pmm_enable_physmap(void);

// Аллокация физических страниц (возвращает физический адрес)
uint64_t pmm_alloc_phys(size_t pages);

// Освобождение физических страниц
void pmm_free_phys(uint64_t phys_addr, size_t pages);

// Статистика
size_t pmm_get_free_pages(void);
size_t pmm_get_total_pages(void);
