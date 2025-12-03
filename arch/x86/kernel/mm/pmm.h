#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Розмір сторінки: 4KB
#define PAGE_SIZE 4096

// Макроси для вирівнювання адрес
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

// Структура для інформації про PMM
typedef struct
{
	uint64_t total_memory;	// Загальний обсяг пам'яті в байтах
	uint64_t usable_memory; // Доступна пам'ять
	uint64_t used_memory;	// Використана пам'ять
	uint64_t total_pages;	// Загальна кількість сторінок
	uint64_t used_pages;	// Кількість використаних сторінок
	uint8_t *bitmap;	// Вказівник на bitmap
	uint64_t bitmap_size;	// Розмір bitmap в байтах
} pmm_info_t;

/**
 * Ініціалізує Physical Memory Manager
 * @param heap_start Початок heap-області з bootloader'а
 * @param heap_size Розмір heap-області
 */
void pmm_init(uint64_t heap_start, uint64_t heap_size);

/**
 * Виділяє одну фізичну сторінку (4KB)
 * @return Фізична адреса виділеної сторінки або 0 при помилці
 */
uint64_t pmm_alloc_page(void);

/**
 * Виділяє кілька послідовних фізичних сторінок
 * @param count Кількість сторінок
 * @return Фізична адреса першої сторінки або 0 при помилці
 */
uint64_t pmm_alloc_pages(size_t count);

/**
 * Звільняє одну фізичну сторінку
 * @param addr Фізична адреса сторінки
 */
void pmm_free_page(uint64_t phys_addr);

/**
 * Звільняє кілька послідовних фізичних сторінок
 * @param addr Фізична адреса першої сторінки
 * @param count Кількість сторінок
 */
void pmm_free_pages(uint64_t phys_addr, size_t count);
