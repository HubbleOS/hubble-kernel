#include <hubble/syscall.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <msr.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#define PROT_READ 0x1  // можна читати
#define PROT_WRITE 0x2 // можна писати
#define PROT_EXEC 0x4  // можна виконувати як інструкції процесора
#define PROT_NONE 0x0  // взагалі ніякого доступу

long sys_mprotect(void *addr, size_t len, int prot) {
  task_t *p = get_current_task();
  uint64_t start = PAGE_ALIGN_DOWN((uint64_t)addr);
  uint64_t end = PAGE_ALIGN_UP((uint64_t)addr + len);

  uint64_t flags = PTE_PRESENT | PTE_USER;
  if (prot & PROT_WRITE)
    flags |= PTE_WRITE;
  if (!(prot & PROT_EXEC))
    flags |= PTE_NX; // якщо є NX-біт підтримка

  for (uint64_t va = start; va < end; va += PAGE_SIZE) {
    // if (vmm_update_page_flags(p->page_table, va, flags) < 0)
    //     return -1; // -EINVAL/-ENOMEM залежно від причини
  }
  return 0;
}
