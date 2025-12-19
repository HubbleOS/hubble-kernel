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

/** Direct physical memory map base (for high addresses) */
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL

// ============================================================================
// Address Conversion Macros
// ============================================================================

#define VIRT_TO_PHYS(addr) ((uint64_t)((uintptr_t)(addr) - KERNEL_VIRT_BASE))

// Simple version for low memory (< 4GB)
// Used by bootloader and for kernel image itself
#define PHYS_TO_VIRT(addr) ((uint64_t)(addr) + KERNEL_VIRT_BASE)

// For pointer types
#define PHYS_TO_VIRT_PTR(type, addr) ((type *)((uintptr_t)(addr) + KERNEL_VIRT_BASE))

// For high MMIO addresses (like LAPIC at 0xFEE00000)
// Use direct map region
#define PHYS_TO_VIRT_MMIO(addr) ((uint64_t)(addr) + DIRECT_MAP_BASE)
#define PHYS_TO_VIRT_MMIO_PTR(type, addr) ((type *)((uintptr_t)(addr) + DIRECT_MAP_BASE))

#define IS_KERNEL_VIRT(addr) ((uint64_t)(addr) >= KERNEL_VIRT_BASE)
#define IS_PHYSICAL(addr) ((uint64_t)(addr) < KERNEL_VIRT_BASE)

// Check if address is in high MMIO range (typically 0xFE000000-0xFFFFFFFF)
#define IS_HIGH_MMIO(addr) ((uint64_t)(addr) >= 0xFE000000ULL && \
			    (uint64_t)(addr) < 0x100000000ULL)
