/**
 * @file higher_half.h
 * @brief Unified memory layout definitions for bootloader and kernel
 *
 * Defines the physical and virtual memory layout constants and
 * conversion helpers used to translate between physical addresses
 * and higher-half kernel virtual addresses.
 */

#pragma once

#include <stdint.h>

/* ── Physical Memory Layout ──────────────────────────────────── */

#define KERNEL_PHYS_BASE 0x100000ULL

/* ── Virtual Memory Layout ───────────────────────────────────── */

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL
#define DIRECT_MAP_BASE 0xFFFF800000000000ULL

/**
 * @brief Convert a physical address to a kernel virtual address
 *
 * @param phys Physical address
 * @return Virtual address in the direct map region
 */
static inline uint64_t phys_to_virt(uint64_t phys)
{
	return phys + DIRECT_MAP_BASE;
}

/**
 * @brief Convert a virtual address back to a physical address
 *
 * Handles both higher-half kernel addresses and direct-map addresses.
 *
 * @param virt Virtual address
 * @return Corresponding physical address
 */
static inline uint64_t virt_to_phys(uint64_t virt)
{
	if (virt >= KERNEL_VIRT_BASE)
		return virt - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;

	if (virt >= DIRECT_MAP_BASE)
		return virt - DIRECT_MAP_BASE;

	return virt;
}
