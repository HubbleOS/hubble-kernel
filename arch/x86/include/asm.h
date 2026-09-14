/**
 * @file asm.h
 * @brief Assembly helper functions for x86 architecture
 *
 * Includes inline assembly wrappers for common x86 operations
 * such as CR3 access, TLB flushing, interrupt control, and
 * CPU hints.
 */

#pragma once

#include <stdint.h>

/* -- Page Table Control ---------------------------------------- */

/**
 * @brief Get the current CR3 value (physical address of PML4)
 *
 * @return Physical address of the current top-level page table
 */
static inline uint64_t get_cr3(void) {
  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

/**
 * @brief Set CR3 to switch page tables
 *
 * @param pml4_phys Physical address of the new PML4
 */
static inline void set_cr3(uint64_t pml4_phys) {
  asm volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

/* -- TLB Management -------------------------------------------- */

/**
 * @brief Flush a single TLB entry for a given virtual address
 *
 * @param virt Virtual address to invalidate
 */
static inline void invlpg(void *virt) {
  asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* -- Interrupt Control ----------------------------------------- */

/**
 * @brief Enable CPU interrupts (set IF flag)
 */
static inline void sti(void) { asm volatile("sti"); }

/**
 * @brief Disable CPU interrupts (clear IF flag)
 */
static inline void cli(void) { asm volatile("cli"); }

/**
 * @brief Save RFLAGS and disable interrupts
 *
 * Pair with restore_flags() to make a critical section safe against
 * nesting: restoring the saved flags leaves interrupts disabled if they
 * already were (e.g. called from within another such section), instead
 * of unconditionally re-enabling them like a bare sti() would.
 *
 * @return Previous RFLAGS value
 */
static inline uint64_t save_flags_cli(void) {
  uint64_t flags;
  asm volatile("pushfq; pop %0; cli" : "=r"(flags)::"memory");
  return flags;
}

/**
 * @brief Restore RFLAGS previously saved by save_flags_cli()
 *
 * @param flags Value returned by save_flags_cli()
 */
static inline void restore_flags(uint64_t flags) {
  asm volatile("push %0; popfq" ::"r"(flags) : "memory", "cc");
}

/* -- CPU Hints ------------------------------------------------- */

/**
 * @brief Halt the CPU until the next interrupt
 */
static inline void hlt(void) { asm volatile("hlt"); }

/**
 * @brief PAUSE hint for spin-wait loops
 *
 * Reduces power consumption on SMT CPUs during busy-waiting.
 */
static inline void cpu_pause(void) { asm volatile("pause"); }
