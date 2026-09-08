/**
 * @file higher_half.h
 * @brief Memory layout definitions for the kernel
 *
 * Defines virtual-to-physical address conversion helpers.
 * The HHDM (Higher Half Direct Map) offset comes from Limine
 * at runtime. The kernel virtual base is from the linker script.
 */

#pragma once

#include <stdint.h>

#include <boot/limine.h>
#include <requests.h>

/* -- Virtual Memory Layout ----------------------------------------------- */

#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

/**
 * @brief Get the HHDM offset from Limine at runtime
 */
static inline uint64_t get_hhdm_offset(void) {
  if (limine_hhdm_req.response)
    return limine_hhdm_req.response->offset;
  return 0xFFFF800000000000ULL; /* fallback */
}

/**
 * @brief Convert a physical address to a direct-map virtual address
 */
static inline uint64_t phys_to_virt(uint64_t phys) {
  return phys + get_hhdm_offset();
}

/**
 * @brief Convert a direct-map virtual address to a physical address
 */
static inline uint64_t virt_to_phys(uint64_t virt) {
  uint64_t hhdm = get_hhdm_offset();
  if (virt >= KERNEL_VIRT_BASE) {
    /* Kernel virtual address — use Limine-provided physical base */
    if (limine_exec_addr_req.response)
      return virt - KERNEL_VIRT_BASE +
             limine_exec_addr_req.response->physical_base;
    /* Fallback: assume kernel loaded at HHDM offset - KERNEL_VIRT_BASE + 0
     * This is wrong but avoids a crash if exec_addr is unavailable */
    return virt - KERNEL_VIRT_BASE;
  }
  if (virt >= hhdm)
    return virt - hhdm;
  return virt;
}

/**
 * @brief Check if a virtual address is in the HHDM direct map
 */
static inline bool is_direct_map(uint64_t addr) {
  return addr >= get_hhdm_offset() && addr != 0;
}
