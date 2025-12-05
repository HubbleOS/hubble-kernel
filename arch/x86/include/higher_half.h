/**
 * @file higher_half.h
 * @brief Unified memory layout definitions for bootloader and kernel
 */

#pragma once

#include <stdint.h>

// ============================================================================
// Physical Memory Layout
// ============================================================================

/** Kernel physical load address (1MB) */
#define KERNEL_PHYS_BASE 0x100000ULL

// ============================================================================
// Virtual Memory Layout
// ============================================================================

/** Higher-half kernel base address */
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

/** Alias for bootloader compatibility */
#define HIGHER_HALF_BASE KERNEL_VIRT_BASE

// ============================================================================
// Address Conversion Macros
// ============================================================================

#define VIRT_TO_PHYS(addr) ((uint64_t)((uintptr_t)(addr) - KERNEL_VIRT_BASE))

#define PHYS_TO_VIRT(addr) (uint64_t)((void *)((uint64_t)(addr) + KERNEL_VIRT_BASE))  // для чисел
#define PHYS_TO_VIRT_PTR(type, addr) ((type *)((uintptr_t)(addr) + KERNEL_VIRT_BASE)) // для pointer

#define IS_KERNEL_VIRT(addr) ((uint64_t)(addr) >= KERNEL_VIRT_BASE)
#define IS_PHYSICAL(addr) ((uint64_t)(addr) < KERNEL_VIRT_BASE)
