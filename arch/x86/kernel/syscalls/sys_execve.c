/*
 * Syscall: execve.
 *
 * QUICK TEST-QUALITY IMPLEMENTATION - just enough to smoke-test a
 * fork()+execve() flow, not a correct POSIX execve() yet. Known shortcuts:
 *
 *  - the old address space (page table, vm_map) is leaked instead of torn
 *    down - fine for a one-shot test process, not for repeated execve().
 *  - argv/envp are passed straight through to execv() as kernel pointers;
 *    there's no copy-in from user space, so callers must currently pass
 *    something the kernel can dereference directly.
 *  - fd table / signal state aren't reset (POSIX wants close-on-exec
 *    handled here; not done).
 *
 * Remove this comment once execve is fleshed out into the real thing.
 */

#include <hubble/errno.h>
#include <hubble/syscalls.h>
#include <msr.h>

#include <smp/scheduler.h>
#include <smp/task.h>
#include <user/exec.h>

#include "syscall_entry.h"
#include <asm.h>

/**
 * @brief execve() - replace the current task's program image.
 *
 * Loads the new ELF as a throwaway task_t via the existing execv() path
 * (reuses all of the page-table/ELF-load/stack-build machinery as-is),
 * then splices its address space and entry state onto the CURRENTLY
 * RUNNING task instead of scheduling it separately, and redirects the
 * pending sysret to the new entry point/stack.
 *
 * Special-cased on the raw trap frame (like sys_fork) because a
 * successful execve doesn't return a value through RAX - it has to
 * rewrite where the syscall returns TO.
 *
 * @param regs Trap frame from syscall_entry.asm.
 * @return Negative errno on failure. On success this never actually
 *         returns to the caller - the trap frame now points at the new
 *         program - but the value handed back to syscall_handler_wrapper
 *         is unused for that case.
 */
long sys_execve(registers_t *regs) {
  const char *path = (const char *)regs->rdi;

  /* No copy-in from user space yet (see file header): argv/envp are
   * pointers into the CALLING process's OLD address space, and execv()
   * only dereferences them after switching CR3 to the freshly loaded
   * image's page table, where they're not valid. Ignoring whatever the
   * caller passed and forcing NULL here just falls back to execv()'s own
   * default_argv/default_envp (which only touches `path`, already proven
   * dereferenceable since elf_load() got this far with it) - good enough
   * for a quick smoke test, wrong for a real execve(argv, envp). */
  char *const *argv = NULL;
  char *const *envp = NULL;

  if (!path)
    return -EINVAL;

  task_t *image = execv(path, argv, envp);
  if (!image)
    return -ENOEXEC;

  task_t *current = get_current_task();

  /* TODO: tear down current->mm.page_table / vm_map instead of leaking
   * them once execve needs to survive more than a quick test. */
  current->mm.page_table = image->mm.page_table;
  current->mm.vm_map = image->mm.vm_map;
  current->mm.heap_start = image->mm.heap_start;
  current->mm.heap_end = image->mm.heap_end;
  current->mm.fs_base = image->mm.fs_base;
  current->mm.tls_size = image->mm.tls_size;

  asm volatile("mov %0, %%cr3" ::"r"(current->mm.page_table) : "memory");
  if (current->mm.fs_base)
    wrmsr(0xC0000100, current->mm.fs_base);

  uint64_t new_rip = image->exec.context.rip;
  uint64_t new_rsp = image->exec.context.rsp;

  /* Only image->mm/exec.context were harvested above; the throwaway
   * task_t (and its own kernel stack) is discarded - leaked, not freed,
   * for now. */
  kfree(image);

  /* Redirect the pending sysret instead of returning normally: RCX holds
   * the return RIP (syscall_entry.asm restores it with "pop rcx" right
   * before sysret), while the saved user RSP lives at %gs:8 (restored by
   * "mov rsp, [gs:8]" just before sysret too) rather than in the register
   * frame at all - so that's the one we have to poke directly. */
  regs->rcx = new_rip;
  asm volatile("mov %0, %%gs:8" ::"r"(new_rsp) : "memory");

  return 0;
}
