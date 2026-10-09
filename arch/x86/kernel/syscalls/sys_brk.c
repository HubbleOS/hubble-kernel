#include <mm/kmalloc.h>
#include <mm/map/vm_map.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <smp/scheduler.h>
#include <smp/task.h>
#include <stdint.h>

#include <hubble/syscalls.h>

#include "syscall_entry.h"

/* Keep the heap's pages in the task's VMA list: fork() copies exactly what
 * the VMAs describe, and a forked child that ran on the parent's heap
 * (a shell's pipeline stage, say) faulted on every heap access. */
static void heap_set_top(task_t *p, uint64_t top) {
  vm_area_t *heap = vm_find_area(p->mm.vm_map, p->mm.heap_start);

  if (top <= p->mm.heap_start) {
    if (heap) {
      vm_remove_area(p->mm.vm_map, heap);
      kfree(heap);
    }
    return;
  }

  if (!heap) {
    heap = kmalloc(sizeof(vm_area_t), GFP_ZERO);
    if (!heap)
      return;
    heap->base = p->mm.heap_start;
    heap->flags = VM_READ | VM_WRITE;
    heap->type = VMA_ANONYMOUS;
    heap->size = top - heap->base;
    vm_insert_area(p->mm.vm_map, heap);
    return;
  }
  heap->size = top - heap->base;
}

// syscall 12
uint64_t sys_brk(uint64_t new_addr) {
  task_t *p = get_current_task();

  if (new_addr == 0)
    return p->mm.heap_end;

  if (new_addr < p->mm.heap_start)
    return p->mm.heap_end;

  //   if (p->heap_max && new_addr > p->heap_max)
  //     return p->heap_end;

  uint64_t old_end = p->mm.heap_end;
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
      if (vmm_map_page_into(p->mm.page_table, va, (uint64_t)phys,
                            PTE_PRESENT | PTE_WRITE | PTE_USER) < 0) {
        // free_frame(phys);
        return old_end;
      }
    }
  } else if (new_top < old_top) {
    for (uint64_t va = new_top; va < old_top; va += PAGE_SIZE) {
      uint64_t phys = vmm_get_phys_from(p->mm.page_table, va);
      //   unmap_page(p->page_table, va);
      //   free_frame((void *)phys);
    }
  }

  if (new_top != old_top)
    heap_set_top(p, new_top);

  p->mm.heap_end = new_addr;
  return p->mm.heap_end;
}
