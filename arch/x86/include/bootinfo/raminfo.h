/**
 * @file raminfo.h
 * @brief Boot-time memory layout information
 *
 * Passed from the bootloader to the kernel to describe the
 * initial memory layout, including heap region and boot page table.
 */

#pragma once

#include <stdint.h>

/**
 * @brief Boot-time memory layout descriptor
 */
typedef struct {
  uint64_t heap_start;
  uint64_t heap_size;
  uint64_t pml4_phys;
} ram_info_t;
