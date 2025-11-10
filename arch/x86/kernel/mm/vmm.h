#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Page flags (біти у PTE)
#define PAGE_PRESENT (1ULL << 0)       // Сторінка присутня у пам'яті
#define PAGE_WRITE (1ULL << 1)	       // Дозволений запис
#define PAGE_USER (1ULL << 2)	       // Доступ для user mode
#define PAGE_WRITETHROUGH (1ULL << 3)  // Write-through caching
#define PAGE_CACHE_DISABLE (1ULL << 4) // Вимкнути кешування
#define PAGE_ACCESSED (1ULL << 5)      // Сторінка була доступна
#define PAGE_DIRTY (1ULL << 6)	       // Сторінка була змінена
#define PAGE_HUGE (1ULL << 7)	       // Huge page (2MB/1GB)
#define PAGE_GLOBAL (1ULL << 8)	       // Глобальна сторінка
#define PAGE_NX (1ULL << 63)	       // No Execute

// Стандартні комбінації флагів
#define PAGE_KERNEL (PAGE_PRESENT | PAGE_WRITE)
#define PAGE_USERSPACE (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)
#define PAGE_READONLY (PAGE_PRESENT)

// Розміри сторінок
#define PAGE_SIZE_4K 0x1000
#define PAGE_SIZE_2M 0x200000
#define PAGE_SIZE_1G 0x40000000

// Маски для Page Table Entry
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define PTE_FLAGS_MASK 0xFFF0000000000FFFULL

// Індекси у Page Table
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr) (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr) (((addr) >> 12) & 0x1FF)

// Адреса рекурсивного мапінгу (останній запис PML4)
#define RECURSIVE_INDEX 511
#define RECURSIVE_BASE 0xFFFFFF8000000000ULL

// Адреси для доступу до page tables через рекурсивний мапінг
#define PML4_VADDR (RECURSIVE_BASE | (RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) | (RECURSIVE_INDEX << 21) | (RECURSIVE_INDEX << 12))
#define PDPT_VADDR(i) (RECURSIVE_BASE | (RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) | (RECURSIVE_INDEX << 21) | ((i) << 12))
#define PD_VADDR(i, j) (RECURSIVE_BASE | (RECURSIVE_INDEX << 39) | (RECURSIVE_INDEX << 30) | ((i) << 21) | ((j) << 12))
#define PT_VADDR(i, j, k) (RECURSIVE_BASE | (RECURSIVE_INDEX << 39) | ((i) << 30) | ((j) << 21) | ((k) << 12))

// Структура для Page Table Entry
typedef uint64_t pte_t;

// Структура для Page Table
typedef struct
{
	pte_t entries[512];
} __attribute__((aligned(4096))) page_table_t;

// Структура адресного простору
typedef struct address_space
{
	uint64_t pml4_phys;	 // Фізична адреса PML4
	page_table_t *pml4_virt; // Віртуальна адреса PML4 (якщо змаповано)
	uint64_t heap_start;	 // Початок user heap
	uint64_t heap_end;	 // Кінець user heap
	uint64_t stack_start;	 // Початок user stack
	uint64_t stack_end;	 // Кінець user stack
} address_space_t;

/**
 * Ініціалізує VMM та створює kernel page tables
 * @param bootloader_pml4_phys Фізична адреса PML4 створеної bootloader'ом
 */
void vmm_init(uint64_t bootloader_pml4_phys);

/**
 * Створює новий адресний простір (для процесу)
 * @return Вказівник на address_space або NULL при помилці
 */
address_space_t *vmm_create_address_space(void);

/**
 * Видаляє адресний простір
 * @param as Вказівник на address_space
 */
void vmm_destroy_address_space(address_space_t *as);

/**
 * Перемикається на інший адресний простір
 * @param as Вказівник на address_space
 */
void vmm_switch_address_space(address_space_t *as);

/**
 * Отримує поточний адресний простір
 * @return Вказівник на поточний address_space
 */
address_space_t *vmm_get_current_address_space(void);

/**
 * Мапить фізичну сторінку на віртуальну адресу
 * @param virt Віртуальна адреса
 * @param phys Фізична адреса
 * @param flags Флаги сторінки (PAGE_*)
 * @return true при успіху, false при помилці
 */
bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

/**
 * Анмапить віртуальну сторінку
 * @param virt Віртуальна адреса
 */
void vmm_unmap_page(uint64_t virt);

/**
 * Отримує фізичну адресу для віртуальної
 * @param virt Віртуальна адреса
 * @return Фізична адреса або 0 якщо не змаповано
 */
uint64_t vmm_virt_to_phys(uint64_t virt);

/**
 * Мапить діапазон сторінок
 * @param virt_start Початкова віртуальна адреса
 * @param phys_start Початкова фізична адреса
 * @param size Розмір у байтах
 * @param flags Флаги сторінок
 * @return true при успіху
 */
bool vmm_map_range(uint64_t virt_start, uint64_t phys_start, uint64_t size, uint64_t flags);

/**
 * Анмапить діапазон сторінок
 * @param virt_start Початкова віртуальна адреса
 * @param size Розмір у байтах
 */
void vmm_unmap_range(uint64_t virt_start, uint64_t size);

/**
 * Виділяє віртуальну пам'ять (знаходить вільний діапазон)
 * @param size Розмір у байтах
 * @param flags Флаги сторінок
 * @return Віртуальна адреса або 0 при помилці
 */
uint64_t vmm_alloc(uint64_t size, uint64_t flags);

/**
 * Звільняє віртуальну пам'ять
 * @param virt Віртуальна адреса
 * @param size Розмір у байтах
 */
void vmm_free(uint64_t virt, uint64_t size);

/**
 * Перевіряє чи змаповано сторінку
 * @param virt Віртуальна адреса
 * @return true якщо сторінка присутня
 */
bool vmm_is_mapped(uint64_t virt);

/**
 * Змінює флаги сторінки
 * @param virt Віртуальна адреса
 * @param flags Нові флаги
 * @return true при успіху
 */
bool vmm_set_flags(uint64_t virt, uint64_t flags);

/**
 * Отримує флаги сторінки
 * @param virt Віртуальна адреса
 * @return Флаги або 0 якщо не змаповано
 */
uint64_t vmm_get_flags(uint64_t virt);

/**
 * Інвалідує TLB для сторінки
 * @param virt Віртуальна адреса
 */
static inline void vmm_invlpg(uint64_t virt)
{
	asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/**
 * Перезавантажує CR3 (інвалідує весь TLB)
 */
static inline void vmm_reload_cr3(void)
{
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/**
 * Отримує поточне значення CR3
 */
static inline uint64_t vmm_get_cr3(void)
{
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	return cr3;
}

/**
 * Встановлює значення CR3
 */
static inline void vmm_set_cr3(uint64_t cr3)
{
	asm volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/**
 * Друкує статистику VMM
 */
void vmm_print_stats(void);
