/**
 * @file vmm.c
 * @brief Virtual Memory Manager implementation
 *
 * Provides page table management using recursive page tables.
 * Supports kernel and user-space virtual address mappings.
 */

#include <hubble/printk.h>
#include <hubble/string.h>

#include "asm.h"
#include "higher_half.h"

#include "pmm.h"
#include "vmm.h"

/* -- Global State ---------------------------------------------------------- */

vmm_info_t g_vmm = {0};

/* -- Internal Helpers ------------------------------------------------------ */

/**
 * @brief Extract physical address from a page table entry
 *
 * @param entry Page table entry
 * @return Physical address
 */
static inline uint64_t pte_addr(uint64_t entry) {
  return entry & 0x000FFFFFFFFFF000ULL;
}

/**
 * @brief Check if a page table entry has the PRESENT bit set
 *
 * @param entry Page table entry
 * @return true if present
 */
static inline bool pte_present(uint64_t entry) { return entry & PTE_PRESENT; }

/**
 * @brief Create a page table entry from a physical address and flags
 *
 * @param pa Physical address
 * @param flags PTE flags
 * @return Page table entry value
 */
static inline uint64_t pte_make(uint64_t pa, uint64_t flags) {
  return (pa & 0x000FFFFFFFFFF000ULL) | flags;
}

/**
 * @brief Allocate and zero a page for use as a page table
 *
 * @return Virtual address of the new table, or NULL on failure
 */
static uint64_t *vmm_alloc_table(void) {
  uint64_t phys = pmm_alloc_page();
  if (!phys) {
    printk(KERN_ERR "Failed to allocate page for VMM table\n");
    return NULL;
  }
  uint64_t *virt = (uint64_t *)phys_to_virt(phys);
  memset(virt, 0, VMM_PAGE_SIZE);
  return virt;
}

/* -- Physical Address Translation ------------------------------------------ */

/**
 * @brief Get the physical address mapped to a virtual address
 *
 * @param va Virtual address
 * @return Physical address, or 0 if not mapped
 */
uint64_t vmm_get_phys(uint64_t va) {
  uint64_t *pt = pt_table(va);
  if (!pte_present(pt[PT_INDEX(va)]))
    return 0;
  return pte_addr(pt[PT_INDEX(va)]);
}

/* -- Initialization -------------------------------------------------------- */

/**
 * @brief Initialize the Virtual Memory Manager
 */
void vmm_init(void) {
  g_vmm.pml4_phys = get_cr3();
  g_vmm.pml4_virt = pml4_table();
  g_vmm.total_mapped_pages = 0;
}

/**
 * @brief Check if a physical address is in the MMIO region
 *
 * @param pa Physical address
 * @return true if in MMIO range
 */
bool is_mmio(uint64_t pa) { return pa >= 0xFEC00000; }

/* -- Page Mapping ---------------------------------------------------------- */

/**
 * @brief Map a virtual page to a physical page
 *
 * Walks the page table hierarchy, allocating intermediate tables as needed.
 * Supports 2 MB huge pages for kernel mappings and 4 KB pages for userspace.
 *
 * @param va Virtual address
 * @param pa Physical address
 * @param flags Page table entry flags
 * @return 0 on success, -1 on failure
 */
int vmm_map_page(uint64_t va, uint64_t pa, uint64_t flags) {
  uint64_t *pml4 = pml4_table();

  uint64_t table_flags = PTE_PRESENT | PTE_WRITE;
  if (flags & PTE_USER)
    table_flags |= PTE_USER;

  if (!pte_present(pml4[PML4_INDEX(va)])) {
    uint64_t *new_pdpt = vmm_alloc_table();
    if (!new_pdpt) {
      printk(KERN_ERR "[VMM] ERROR: Failed to allocate new PDPT\n");
      return -1;
    }
    pml4[PML4_INDEX(va)] =
        pte_make(virt_to_phys((uint64_t)new_pdpt), table_flags);
  } else {
    if ((flags & PTE_USER) && !(pml4[PML4_INDEX(va)] & PTE_USER))
      pml4[PML4_INDEX(va)] |= PTE_USER;
  }

  uint64_t *pdpt = pdpt_table(va);
  if (!pte_present(pdpt[PDPT_INDEX(va)])) {
    uint64_t *new_pd = vmm_alloc_table();
    if (!new_pd) {
      printk(KERN_ERR "[VMM] ERROR: Failed to allocate new PD\n");
      return -1;
    }
    pdpt[PDPT_INDEX(va)] =
        pte_make(virt_to_phys((uint64_t)new_pd), table_flags);
  } else {
    if ((flags & PTE_USER) && !(pdpt[PDPT_INDEX(va)] & PTE_USER))
      pdpt[PDPT_INDEX(va)] |= PTE_USER;
  }

  uint64_t *pd = pd_table(va);

  if ((flags & PTE_USER) == 0 && !is_mmio(pa) &&
      (pa % VMM_HUGE_PAGE_SIZE == 0) && (va % VMM_HUGE_PAGE_SIZE == 0)) {
    pd[PD_INDEX(va)] = pte_make(pa, flags | PTE_HUGE);
    g_vmm.total_mapped_pages += VMM_HUGE_PAGE_SIZE / VMM_PAGE_SIZE;
    invlpg((void *)va);
    printk(KERN_INFO "[VMM] Mapping huge page 0x%lx to 0x%lx\n", va, pa);
    return 0;
  }

  if (!pte_present(pd[PD_INDEX(va)])) {
    uint64_t *new_pt = vmm_alloc_table();
    if (!new_pt) {
      printk(KERN_ERR "[VMM] ERROR: Failed to allocate new PT\n");
      return -1;
    }
    pd[PD_INDEX(va)] = pte_make(virt_to_phys((uint64_t)new_pt), table_flags);
  } else {
    if (pd[PD_INDEX(va)] & PTE_HUGE) {
      printk(KERN_ERR "[VMM] ERROR: Huge page already mapped at 0x%lx\n", va);
      return -1;
    }

    if ((flags & PTE_USER) && !(pd[PD_INDEX(va)] & PTE_USER))
      pd[PD_INDEX(va)] |= PTE_USER;
  }

  uint64_t pte_flags = PTE_PRESENT | PTE_WRITE;

  if (flags & VMM_MAP_NO_CACHE)
    pte_flags |= PTE_PCD | PTE_PWT;

  if ((flags & VMM_MAP_GLOBAL) && !is_mmio(pa))
    pte_flags |= PTE_GLOBAL;

  if (flags & VMM_MAP_USER)
    pte_flags |= PTE_USER;

  if (flags & PTE_USER)
    pte_flags |= PTE_USER;

  uint64_t *pt = pt_table(va);
  pt[PT_INDEX(va)] = pte_make(pa, pte_flags);

  g_vmm.total_mapped_pages++;
  invlpg((void *)va);
  return 0;
}

/* -- Page Unmapping -------------------------------------------------------- */

/**
 * @brief Unmap a virtual page
 *
 * @param va Virtual address to unmap
 */
void vmm_unmap_page(uint64_t va) {
  uint64_t *pt = pt_table(va);
  if (!pte_present(pt[PT_INDEX(va)])) {
    printk(KERN_ERR "[VMM] ERROR: Page not mapped at 0x%lx\n", va);
    return;
  }
  pt[PT_INDEX(va)] = 0;
  g_vmm.total_mapped_pages--;
  invlpg((void *)va);
}

/* -- Page Flag Manipulation ------------------------------------------------ */

/**
 * @brief Change page table entry flags for an existing mapping
 *
 * @param va Virtual address
 * @param flags New PTE flags
 * @return 0 on success, -1 if not mapped
 */
int vmm_set_flags(uint64_t va, uint64_t flags) {
  uint64_t *pt = pt_table(va);
  if (!pte_present(pt[PT_INDEX(va)]))
    return -1;
  uint64_t pa = pte_addr(pt[PT_INDEX(va)]);
  pt[PT_INDEX(va)] = pte_make(pa, flags);
  invlpg((void *)va);
  return 0;
}

/**
 * @brief Check if a virtual address is mapped
 *
 * @param va Virtual address
 * @return true if mapped
 */
bool vmm_is_mapped(uint64_t va) {
  uint64_t *pt = pt_table(va);
  return pte_present(pt[PT_INDEX(va)]);
}

/* -- User Page Management -------------------------------------------------- */

/**
 * @brief Unmap a user page, walking the hierarchy safely
 *
 * @param va Virtual address to unmap
 */
void vmm_unmap_user_page(uint64_t va) {
  uint64_t *pml4 = pml4_table();
  if (!pte_present(pml4[PML4_INDEX(va)]))
    return;

  uint64_t *pdpt = pdpt_table(va);
  if (!pte_present(pdpt[PDPT_INDEX(va)]))
    return;

  uint64_t *pd = pd_table(va);
  if (!pte_present(pd[PD_INDEX(va)]))
    return;

  if (pd[PD_INDEX(va)] & PTE_HUGE) {
    pd[PD_INDEX(va)] = 0;
    invlpg((void *)va);
    return;
  }

  uint64_t *pt = pt_table(va);
  if (!pte_present(pt[PT_INDEX(va)]))
    return;

  pt[PT_INDEX(va)] = 0;
  g_vmm.total_mapped_pages--;
  invlpg((void *)va);
}

/* -- Debug Dump Functions -------------------------------------------------- */

/**
 * @brief Dump page table entry flags for a virtual address
 *
 * @param va Virtual address
 */
void dump_page_flags(uint64_t va) {
  uint64_t *pt = pt_table(va);
  uint64_t pte = pt[PT_INDEX(va)];
  printk(KERN_INFO "VA 0x%lx -> PTE 0x%lx\n", va, pte);

  printk(KERN_INFO "Flags: PRESENT=%d USER=%d WRITE=%d NX=%d\n",
         !!(pte & PTE_PRESENT), !!(pte & PTE_USER), !!(pte & PTE_WRITE),
         !!(pte & PTE_NX));
}

/**
 * @brief Dump memory content at a virtual address
 *
 * @param va Virtual address
 * @param len Number of bytes to dump
 */
void dump_page(uint64_t va, size_t len) {
  uint64_t phys = vmm_get_phys(va);
  if (!phys) {
    printk(KERN_ERR "VA 0x%lx not mapped!\n", va);
    return;
  }
  dump_page_flags(va);
  uint8_t *kptr = (uint8_t *)phys_to_virt(phys);
  printk(KERN_INFO "Dumping VA 0x%lx -> PA 0x%lx\n", va, phys);
  for (size_t i = 0; i < len; i++) {
    if (i % 16 == 0)
      printk(KERN_INFO "\n%04zx: ", i);
    printk(KERN_INFO "%02x ", kptr[i]);
  }
  printk(KERN_INFO "\n");
}

/**
 * @brief Set the USER flag on a 2 MB page directory entry
 *
 * @param va Virtual address within the 2 MB page
 * @return 0 on success, -1 on failure
 */
int make_pd_entry_user(uint64_t va) {
  uint64_t cr3 = get_cr3();
  uint64_t pml4_idx = (va >> 39) & 0x1FF;
  uint64_t pdpt_idx = (va >> 30) & 0x1FF;
  uint64_t pd_idx = (va >> 21) & 0x1FF;

  uint64_t *pml4 = (uint64_t *)phys_to_virt(cr3 & ~0xFFFULL);
  uint64_t pml4e = pml4[pml4_idx];
  if (!(pml4e & 1))
    return -1;

  uint64_t *pdpt = (uint64_t *)phys_to_virt(pte_addr(pml4e));
  uint64_t pdpte = pdpt[pdpt_idx];
  if (!(pdpte & 1))
    return -1;

  uint64_t *pd = (uint64_t *)phys_to_virt(pte_addr(pdpte));
  uint64_t pde = pd[pd_idx];

  if (!(pde & (1ULL << 7))) {
    printk(KERN_INFO "Not a large page at PDE\n");
    return -1;
  }

  pd[pd_idx] = pde | (1ULL << 2);
  invlpg((void *)va);
  return 0;
}

/* -- User Page Table Management -------------------------------------------- */

/**
 * @brief Create a user-space page table
 *
 * Copies kernel entries (indices 256-511). User entries (indices 0-255)
 * are left empty — they are populated on demand by elf_load_segment()
 * and sys_mmap() via vmm_map_page_into() / vmm_map_page().
 *
 * @return Physical address of the new PML4, or NULL on failure
 */
uint64_t *vmm_create_user_pagemap(void) {
  uint64_t phys = pmm_alloc_page();
  uint64_t *new_pml4 = (uint64_t *)phys_to_virt(phys);
  memset(new_pml4, 0, PAGE_SIZE);

  uint64_t *old_pml4 = (uint64_t *)phys_to_virt(g_vmm.pml4_phys & ~0xFFFULL);

  for (int i = 256; i < 512; i++) {
    if (i == RECURSIVE_PML4_INDEX)
      continue;
    if (old_pml4[i] & PTE_PRESENT)
      new_pml4[i] = old_pml4[i];
  }

  //   for (int i = 256; i < 512; i++) {
  //     if (i == RECURSIVE_PML4_INDEX)
  //       continue;
  //     if (current_pml4[i] & PTE_PRESENT)
  //       pml4[i] = (current_pml4[i] & ~PTE_WRITE) | PTE_COW;
  //   }

  /* User entries (indices 0-255) are intentionally left empty.
   * The kernel PML4 may contain bootloader identity mappings for low
   * physical memory — those must never appear in a user address space.
   * User mappings are created on demand by elf_load_segment() and
   * sys_mmap() via vmm_map_page_into() / vmm_map_page(). */

  uint64_t new_phys = virt_to_phys((uint64_t)pml4);
  pml4[RECURSIVE_PML4_INDEX] = pte_make(new_phys, PTE_PRESENT | PTE_WRITE);

  return (uint64_t *)new_phys;
}

uint64_t *vmm_copy_user_pagemap(uint64_t *old_phys) {

  uint64_t phys = pmm_alloc_page();
  uint64_t *pml4 = (uint64_t *)phys_to_virt(phys);
  memset(pml4, 0, PAGE_SIZE);

  uint64_t *current_pml4 =
      (uint64_t *)phys_to_virt((uint64_t)old_phys & ~0xFFFULL);

  for (int i = 0; i < 512; i++) {
    if (i == RECURSIVE_PML4_INDEX)
      continue;
    if (current_pml4[i] & PTE_PRESENT)
      pml4[i] = current_pml4[i] & PTE_COW;
  }

  uint64_t new_phys = virt_to_phys((uint64_t)pml4);
  pml4[RECURSIVE_PML4_INDEX] = pte_make(new_phys, PTE_PRESENT | PTE_WRITE);

  return (uint64_t *)new_phys;
}

int vmm_copy_pt(uint64_t *src, uint64_t *trg, int lvl) {

  for (int i = 0; i <= 512; i++) {
    uint64_t entry = src[i];
  }
}

/**
 * @brief Map a page into a specific (non-current) page table
 *
 * Walks the page table hierarchy of the given PML4 and installs a mapping.
 * Handles huge page splitting when an existing 2 MB page needs a 4 KB
 * mapping.
 *
 * @param pml4_phys Physical address of the target PML4
 * @param va Virtual address
 * @param pa Physical address
 * @param flags Page table entry flags
 * @return 0 on success, -1 on failure
 */
int vmm_map_page_into(uint64_t *pml4_phys, uint64_t va, uint64_t pa,
                      uint64_t flags) {
  uint64_t *pml4 = (uint64_t *)phys_to_virt((uint64_t)pml4_phys);
  uint64_t tf = PTE_PRESENT | PTE_WRITE | (flags & PTE_USER ? PTE_USER : 0);

  if (!pte_present(pml4[PML4_INDEX(va)])) {
    uint64_t *t = vmm_alloc_table();
    if (!t)
      return -1;
    pml4[PML4_INDEX(va)] = pte_make(virt_to_phys((uint64_t)t), tf);
  } else if (flags & PTE_USER) {
    pml4[PML4_INDEX(va)] |= PTE_USER;
  }

  uint64_t *pdpt = (uint64_t *)phys_to_virt(pte_addr(pml4[PML4_INDEX(va)]));

  if (!pte_present(pdpt[PDPT_INDEX(va)])) {
    uint64_t *t = vmm_alloc_table();
    if (!t)
      return -1;
    pdpt[PDPT_INDEX(va)] = pte_make(virt_to_phys((uint64_t)t), tf);
  } else if (flags & PTE_USER) {
    pdpt[PDPT_INDEX(va)] |= PTE_USER;
  }

  uint64_t *pd = (uint64_t *)phys_to_virt(pte_addr(pdpt[PDPT_INDEX(va)]));

  if (!pte_present(pd[PD_INDEX(va)])) {
    uint64_t *t = vmm_alloc_table();
    if (!t)
      return -1;
    pd[PD_INDEX(va)] = pte_make(virt_to_phys((uint64_t)t), tf);
  } else if (pd[PD_INDEX(va)] & PTE_HUGE) {
    uint64_t huge_phys = pte_addr(pd[PD_INDEX(va)]);
    uint64_t huge_flags = pd[PD_INDEX(va)] & 0xFFF & ~PTE_HUGE;
    uint64_t *new_pt = vmm_alloc_table();
    if (!new_pt)
      return -1;
    for (int i = 0; i < 512; i++)
      new_pt[i] = pte_make(huge_phys + i * PAGE_SIZE,
                           huge_flags | PTE_PRESENT | PTE_WRITE);
    pd[PD_INDEX(va)] = pte_make(virt_to_phys((uint64_t)new_pt), tf);
  } else if (flags & PTE_USER) {
    pd[PD_INDEX(va)] |= PTE_USER;
  }

  uint64_t *pt = (uint64_t *)phys_to_virt(pte_addr(pd[PD_INDEX(va)]));
  pt[PT_INDEX(va)] = pte_make(pa, flags | PTE_PRESENT);

  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  if ((cr3 & ~0xFFFULL) == (uint64_t)pml4_phys)
    invlpg((void *)va);
  return 0;
}

int vmm_unmap_page_from(uint64_t *pml4_phys, uint64_t va) { return 0; }

/**
 * @brief Get physical address from a specific page table
 *
 * @param pml4_phys Physical address of the PML4
 * @param va Virtual address
 * @return Physical address, or 0 if not mapped
 */
uint64_t vmm_get_phys_from(uint64_t *pml4_phys, uint64_t va) {
  uint64_t *pml4 = (uint64_t *)phys_to_virt((uint64_t)pml4_phys);

  if (!pte_present(pml4[PML4_INDEX(va)]))
    return 0;

  uint64_t *pdpt = (uint64_t *)phys_to_virt(pte_addr(pml4[PML4_INDEX(va)]));
  if (!pte_present(pdpt[PDPT_INDEX(va)]))
    return 0;

  uint64_t *pd = (uint64_t *)phys_to_virt(pte_addr(pdpt[PDPT_INDEX(va)]));
  if (!pte_present(pd[PD_INDEX(va)]))
    return 0;

  uint64_t *pt = (uint64_t *)phys_to_virt(pte_addr(pd[PD_INDEX(va)]));
  if (!pte_present(pt[PT_INDEX(va)]))
    return 0;

  return pte_addr(pt[PT_INDEX(va)]);
}

/**
 * @brief Debug: dump full page table walk for a virtual address
 *
 * @param pml4_phys Physical address of the PML4
 * @param va Virtual address
 */
void debug_dump_mapping(uint64_t *pml4_phys, uint64_t va) {
  uint64_t *pml4 = (uint64_t *)phys_to_virt((uint64_t)pml4_phys);

  uint64_t pml4e = pml4[PML4_INDEX(va)];
  printk(KERN_INFO "PML4[%ld] = 0x%lx  USER=%d WRITE=%d PRESENT=%d\n",
         PML4_INDEX(va), pml4e, !!(pml4e & PTE_USER), !!(pml4e & PTE_WRITE),
         !!(pml4e & PTE_PRESENT));
  if (!pte_present(pml4e))
    return;

  uint64_t *pdpt = (uint64_t *)phys_to_virt(pte_addr(pml4e));
  uint64_t pdpte = pdpt[PDPT_INDEX(va)];
  printk(KERN_INFO "PDPT[%ld] = 0x%lx  USER=%d WRITE=%d PRESENT=%d\n",
         PDPT_INDEX(va), pdpte, !!(pdpte & PTE_USER), !!(pdpte & PTE_WRITE),
         !!(pdpte & PTE_PRESENT));
  if (!pte_present(pdpte))
    return;

  uint64_t *pd = (uint64_t *)phys_to_virt(pte_addr(pdpte));
  uint64_t pde = pd[PD_INDEX(va)];
  printk(KERN_INFO "PD  [%ld] = 0x%lx  USER=%d WRITE=%d PRESENT=%d\n",
         PD_INDEX(va), pde, !!(pde & PTE_USER), !!(pde & PTE_WRITE),
         !!(pde & PTE_PRESENT));
  if (!pte_present(pde))
    return;

  uint64_t *pt = (uint64_t *)phys_to_virt(pte_addr(pde));
  uint64_t pte = pt[PT_INDEX(va)];
  printk(KERN_INFO "PT  [%ld] = 0x%lx  USER=%d WRITE=%d PRESENT=%d NX=%d\n",
         PT_INDEX(va), pte, !!(pte & PTE_USER), !!(pte & PTE_WRITE),
         !!(pte & PTE_PRESENT), !!(pte & PTE_NX));
}

/**
 * @brief Dump the entire kernel page table hierarchy
 */
void dump_kernel_pagemap(void) {
  uint64_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  uint64_t *current_pml4 = (uint64_t *)phys_to_virt(cr3 & ~0xFFFULL);

  printk(KERN_INFO "[VMM] === KERNEL PAGEMAP DUMP");
  for (int i = 0; i < 512; i++) {
    if (!(current_pml4[i] & PTE_PRESENT))
      continue;
    printk(KERN_INFO "[VMM] PML4[%d] = 0x%lx\n", i, current_pml4[i]);

    uint64_t *pdpt = (uint64_t *)phys_to_virt(pte_addr(current_pml4[i]));
    for (int j = 0; j < 512; j++) {
      if (!(pdpt[j] & PTE_PRESENT))
        continue;
      printk(KERN_INFO "[VMM]   PDPT[%d] = 0x%lx\n", j, pdpt[j]);
      if (pdpt[j] & PTE_HUGE)
        continue;

      uint64_t *pd = (uint64_t *)phys_to_virt(pte_addr(pdpt[j]));
      for (int k = 0; k < 512; k++) {
        if (!(pd[k] & PTE_PRESENT))
          continue;
        printk(KERN_INFO "[VMM]     PD[%d] = 0x%lx  HUGE=%d\n", k, pd[k],
               !!(pd[k] & PTE_HUGE));
      }
    }
  }
  printk(KERN_INFO "[VMM] === END DUMP");
}

/**
 * @brief Flush the entire TLB
 */
void flush_tlb(void) { asm volatile("invlpg (%0)" : : "r"(0) : "memory"); }
