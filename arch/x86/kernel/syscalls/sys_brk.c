#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <stdint.h>

#include <hubble/syscalls.h>

#include "syscall_entry.h"

// syscall 12
uint64_t sys_brk(uint64_t new_addr) {
  task_t *p = get_current_task();

  if (new_addr == 0)
    return p->heap_end;

  if (new_addr < p->heap_start)
    return p->heap_end;

  //   if (p->heap_max && new_addr > p->heap_max)
  //     return p->heap_end;

  uint64_t old_end = p->heap_end;
  uint64_t old_top = PAGE_ALIGN_UP(old_end);
  uint64_t new_top = PAGE_ALIGN_UP(new_addr);

  if (new_top > old_top) {
    for (uint64_t va = old_top; va < new_top; va += PAGE_SIZE) {
      void *phys = (void *)(pmm_alloc_page());
      if (!phys) {
        for (uint64_t rollback = old_top; rollback < va; rollback += PAGE_SIZE)
          //   unmap_page(p->page_table,
          //              rollback);
          pmm_free_page((uint64_t)phys);
        return old_end;
      }
      if (vmm_map_page_into(p->page_table, va, (uint64_t)phys,
                            PTE_PRESENT | PTE_WRITE | PTE_USER) < 0) {
        // free_frame(phys);
        return old_end;
      }
    }
  } else if (new_top < old_top) {
    for (uint64_t va = new_top; va < old_top; va += PAGE_SIZE) {
      uint64_t phys = vmm_get_phys_from(
          p->page_table, va); // дістати фіз. адресу перед розмапом
      //   unmap_page(p->page_table, va);
      //   free_frame((void *)phys);
    }
  }

  p->heap_end = new_addr;
  return p->heap_end;
}
