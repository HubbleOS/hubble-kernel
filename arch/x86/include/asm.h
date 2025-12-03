/**
 * @file asm.h
 * @brief  Assembly Helper Functions for x86 Architecture
 *
 * Содержит inline функции для работы с регистрами процессора,
 * такими как CR3, а также для управления TLB.
 */

#pragma once

#include <stdint.h>

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get current CR3 value (physical address of PML4)
 *
 * @return Physical address of current page table
 */
static inline uint64_t get_cr3(void)
{
	uint64_t cr3;
	asm volatile("mov %%cr3, %0" : "=r"(cr3));
	return cr3;
}

/**
 * @brief Set CR3 value (switch page tables)
 *
 * @param pml4_phys Physical address of new PML4
 */
static inline void set_cr3(uint64_t pml4_phys)
{
	asm volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

/**
 * @brief Flush TLB entry for specific virtual address
 *
 * @param virt Virtual address to flush
 */
static inline void invlpg(void *virt)
{
	asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
