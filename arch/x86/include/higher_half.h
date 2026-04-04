/**
 * @file higher_half.h
 * @brief Unified memory layout definitions for bootloader and kernel
 */

#pragma once

#include <stdint.h>

// Physical Memory Layout
#define KERNEL_PHYS_BASE 0x100000ULL // Kernel physical load address

// Virtual Memory Layout
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL // Higher-half kernel base address
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL  // Direct physical memory map base (for high addresses)

static inline uint64_t phys_to_virt(uint64_t phys)
{
	return phys + DIRECT_MAP_BASE;
}

static inline uint64_t virt_to_phys(uint64_t virt)
{
	if (virt >= KERNEL_VIRT_BASE)
		return virt - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;

	if (virt >= DIRECT_MAP_BASE)
		return virt - DIRECT_MAP_BASE;

	return virt;
}
