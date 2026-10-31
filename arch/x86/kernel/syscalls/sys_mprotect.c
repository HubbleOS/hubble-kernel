#include <hubble/errno.h>
#include <hubble/syscalls.h>
#include <mm/map/vm_map.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#include "syscall_entry.h"

#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define USER_END 0x0000800000000000ULL

/* Give the page at @p va the protection of @p vma. A copy-on-write page
 * stays read-only until the write fault copies it (the fault handler
 * checks the VMA allows writing). */
static void protect_page(vm_area_t *vma, uint64_t va) {
  if (!vmm_get_phys(va))
    return;
  uint64_t old = pt_table(va)[PT_INDEX(va)];
  uint64_t flags = vmm_pte_flags(vma);
  if (old & PTE_COW)
    flags = (flags & ~PTE_WRITE) | PTE_COW;
  if (vma->type == VMA_DEVICE)
    flags |= PTE_PCD | PTE_PWT;
  vmm_set_flags(va, flags);
}

/**
 * @brief mprotect: change the protection of [addr, addr + len)
 *
 * Every page in the range must be mapped. VMAs are split at the range's
 * ends, so the new protection is also what fork() copies.
 *
 * @return 0, or -EINVAL (unaligned address, unknown flags) or -ENOMEM
 *         (part of the range not mapped, or no memory to split a VMA)
 */
long sys_mprotect(uint64_t addr, size_t len, int prot) {
  if (addr % PAGE_SIZE || prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
    return -EINVAL;
  uint64_t end = addr + PAGE_ALIGN_UP(len);
  if (end < addr || end > USER_END)
    return -ENOMEM;
  if (end == addr)
    return 0;

  vm_map_t *map = get_current_task()->mm.vm_map;
  if (!map)
    return -ENOMEM;
  for (uint64_t va = addr; va < end;) {
    vm_area_t *vma = vm_find_area(map, va);
    if (!vma)
      return -ENOMEM;
    va = vma->base + vma->size;
  }

  uint32_t vm_flags = 0;
  if (prot & PROT_READ)
    vm_flags |= VM_READ;
  if (prot & PROT_WRITE)
    vm_flags |= VM_WRITE;
  if (prot & PROT_EXEC)
    vm_flags |= VM_EXEC;

  for (uint64_t va = addr; va < end;) {
    vm_area_t *vma = vm_find_area(map, va);
    if (vma->base < va && !(vma = vm_split_area(map, vma, va)))
      return -ENOMEM;
    if (vma->base + vma->size > end && !vm_split_area(map, vma, end))
      return -ENOMEM;
    vma->flags = vm_flags;
    for (uint64_t p = vma->base; p < vma->base + vma->size; p += PAGE_SIZE)
      protect_page(vma, p);
    va = vma->base + vma->size;
  }
  return 0;
}
