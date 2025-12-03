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

#define PHYS_TO_VIRT(addr) ((typeof(addr))((uint64_t)(addr) + KERNEL_VIRT_BASE))
#define VIRT_TO_PHYS(addr) ((typeof(addr))((uint64_t)(addr) - KERNEL_VIRT_BASE))

#define IS_KERNEL_VIRT(addr) ((uint64_t)(addr) >= KERNEL_VIRT_BASE)
#define IS_PHYSICAL(addr) ((uint64_t)(addr) < KERNEL_VIRT_BASE)
