/**
 * @file asm.h
 * @brief  Assembly Helper Functions for x86 Architecture
 *
 * Includes assembly helper functions for x86 architecture
 */

#pragma once

#include <stdint.h>

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

/**
 * @brief Enable CPU interrupts (set IF flag).
 *
 * Allows the CPU to receive maskable hardware interrupts (IRQ).
 * Should be called only after IDT and interrupt controllers are initialized.
 */
static inline void sti(void)
{
	asm volatile("sti");
}

/**
 * @brief Disable CPU interrupts (clear IF flag).
 *
 * Prevents the CPU from receiving maskable interrupts.
 * Useful for critical sections.
 */
static inline void cli(void)
{
	asm volatile("cli");
}

/**
 * @brief Halt CPU until next interrupt.
 *
 * Used for idle loops to reduce CPU usage.
 */
static inline void hlt(void)
{
	asm volatile("hlt");
}

/**
 * @brief CPU hint for spin-wait loops.
 *
 * Reduces power consumption and improves performance
 * in busy-wait (spinlock) loops on SMT CPUs.
 */
static inline void cpu_pause(void)
{
	asm volatile("pause");
}
