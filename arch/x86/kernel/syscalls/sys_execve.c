
#include <hubble/errno.h>
#include <hubble/string.h>
#include <hubble/syscalls.h>
#include <mm/kmalloc.h>
#include <msr.h>

#include <smp/scheduler.h>
#include <smp/task.h>
#include <user/exec.h>

#include "syscall_entry.h"
#include <asm.h>

/**
 * @brief Copy a NULL-terminated argv/envp array (and the strings it
 * points to) from the CALLING process's address space into kernel
 * memory.
 *
 * Must be called before execv() switches CR3 to the freshly loaded
 * image's page table - user_vec and everything it points to belongs to
 * the OLD address space, same reasoning as why execv() itself copies
 * `path` early. The kernel-owned copy this returns lives in the shared
 * kernel half, so it stays dereferenceable after the switch.
 *
 * @param user_vec NULL-terminated array of user pointers, or NULL.
 * @return Kernel-owned NULL-terminated array of kernel-owned strings, or
 *         NULL if user_vec was NULL or the copy failed.
 */
static char **copy_strvec_from_user(char *const *user_vec) {
  if (!user_vec)
    return NULL;

  int count = 0;
  while (user_vec[count])
    count++;

  char **copy = kmalloc(sizeof(char *) * (count + 1), GFP_KERNEL);
  if (!copy)
    return NULL;

  int i;
  for (i = 0; i < count; i++) {
    size_t len = strlen(user_vec[i]) + 1;
    copy[i] = kmalloc(len, GFP_KERNEL);
    if (!copy[i])
      break;
    memcpy(copy[i], user_vec[i], len);
  }
  copy[i] = NULL;
  return copy;
}

/**
 * @brief Free an array returned by copy_strvec_from_user().
 */
static void free_strvec(char **vec) {
  if (!vec)
    return;
  for (int i = 0; vec[i]; i++)
    kfree(vec[i]);
  kfree(vec);
}

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

  if (!path)
    return -EINVAL;

  char **argv = copy_strvec_from_user((char *const *)regs->rsi);
  char **envp = copy_strvec_from_user((char *const *)regs->rdx);

  task_t *image = execv(path, argv, envp);
  if (!image) {
    free_strvec(argv);
    free_strvec(envp);
    return -ENOEXEC;
  }

  free_strvec(argv);
  free_strvec(envp);

  task_t *current = get_current_task();

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

  kfree(image);

  regs->rcx = new_rip;
  asm volatile("mov %0, %%gs:8" ::"r"(new_rsp) : "memory");

  return 0;
}
