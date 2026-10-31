/*
 * Syscalls: fork() and clone() - create a copy-on-write clone of the
 * calling process.
 *
 * The child gets its own task, its own copy of the parent's VMA
 * bookkeeping list, and a page table that shares every physical page with
 * the parent (write-protected + PTE_COW where the mapping was writable).
 * Actual duplication of a shared page happens lazily, in vmm_resolve_cow(),
 * the first time either side writes to it.
 *
 * The child's very first resume must land back in userspace exactly where
 * the parent's `fork()` syscall was made, with a return value of 0 - so
 * this handler is called directly with the raw syscall trap frame instead
 * of going through the generic (num, a1..a6) syscall dispatch.
 */

#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/string.h>

#include <mm/kmalloc.h>
#include <mm/map/vm_map.h>
#include <mm/vmm.h>
#include <smp/fdtable.h>
#include <smp/scheduler.h>
#include <smp/task.h>

#define USER_END 0x0000800000000000ULL

static bool user_int_ptr(const int *p) {
  return p && (uint64_t)p < USER_END - sizeof(int);
}

/**
 * @brief Create the child process
 *
 * @param regs Raw register frame captured at the `syscall` instruction
 * @param newsp The child's stack pointer, or 0 for the parent's
 * @param parent_tid Gets the child's pid in both processes, or NULL
 * @param child_tid Gets the child's pid in the child only, or NULL
 * @return Child PID to the parent (the child itself resumes with 0 in RAX,
 *         set directly in its cloned context rather than returned here)
 */
static long fork_task(registers_t *regs, uint64_t newsp, int *parent_tid,
                      int *child_tid) {
  task_t *parent = get_current_task();

  task_t *child = task_create(NULL, parent->sched.priority, true);
  if (!child)
    return -ENOMEM;

  /* Resume as a copy of the parent's syscall-time registers, not at a
   * fresh entry point. */
  child->exec.context.r15 = regs->r15;
  child->exec.context.r14 = regs->r14;
  child->exec.context.r13 = regs->r13;
  child->exec.context.r12 = regs->r12;
  child->exec.context.r10 = regs->r10;
  child->exec.context.r9 = regs->r9;
  child->exec.context.r8 = regs->r8;
  child->exec.context.rdi = regs->rdi;
  child->exec.context.rsi = regs->rsi;
  child->exec.context.rbp = regs->rbp;
  child->exec.context.rbx = regs->rbx;
  child->exec.context.rdx = regs->rdx;
  child->exec.context.rax = 0; /* fork() returns 0 in the child */

  /* syscall_entry.asm builds a whole frame: where the syscall returns
   * to, with which flags and stack. */
  child->exec.context.rip = regs->rip;
  child->exec.context.rflags = regs->rflags;
  child->exec.context.rsp = newsp ? newsp : regs->rsp;
  child->exec.context.rcx = regs->rcx;
  child->exec.context.r11 = regs->r11;

  child->exec.context.cs = 0x23;
  child->exec.context.ss = 0x1b;
  child->exec.context.ds = 0x1b;
  child->exec.context.es = 0x1b;
  child->exec.context.fs = 0x1b;
  child->exec.context.gs = 0x1b;

  if (child->exec.context.fpu_state)
    asm volatile("fxsave %0" : "=m"(*(char *)child->exec.context.fpu_state));

  /* Copy-on-write address space: share every physical page with the
   * parent, write-protecting both sides' PTEs where the mapping was
   * writable, and give the child its own independent VMA list mirroring
   * it. */
  /* Written before the address space is copied: parent_tid lands in
   * both, and child_tid, put back right after, only in the child's copy. */
  int pid = (int)child->id.pid, saved_child_tid = 0;
  if (parent_tid)
    *parent_tid = pid;
  if (child_tid) {
    saved_child_tid = *child_tid;
    *child_tid = pid;
  }
  child->mm.page_table = vmm_fork_pagemap(parent->mm.vm_map);
  if (child_tid)
    *child_tid = saved_child_tid;
  child->mm.vm_map = vm_map_clone(parent->mm.vm_map);
  if (!child->mm.page_table || !child->mm.vm_map) {
    printk(KERN_ERR "fork: failed to clone address space\n");
    // return -ENOMEM;
  }
  child->mm.heap_start = parent->mm.heap_start;
  child->mm.heap_end = parent->mm.heap_end;

  /* TLS lives in a raw kmalloc'd kernel block pointed to by fs_base, not a
   * page-table mapping - vmm_fork_pagemap()'s COW clone never sees it, so
   * without an explicit copy here the child would run with %fs pointing at
   * the exact same memory as the parent (no COW, no protection at all):
   * musl keeps its stack-protector canary at fs:0x28, so the two tasks
   * would stomp each other's canary and crash into __stack_chk_fail. */
  if (parent->mm.tls_size) {
    void *tls_copy = kmalloc(parent->mm.tls_size, GFP_KERNEL);
    if (!tls_copy) {
      printk(KERN_ERR "fork: failed to duplicate TLS\n");
      return -ENOMEM;
    }
    memcpy(tls_copy, (void *)parent->mm.fs_base, parent->mm.tls_size);
    child->mm.fs_base = (uint64_t)tls_copy;
    child->mm.tls_size = parent->mm.tls_size;
  } else {
    child->mm.fs_base = parent->mm.fs_base;
  }

  child->mm.gs_base = parent->mm.gs_base;

  /* The child's descriptors refer to the parent's open files (shared
   * offsets, as after dup()); each holds its own reference. */
  fd_copy_table(child, parent);

  memcpy(child->id.name, parent->id.name, sizeof(child->id.name));
  child->abi = parent->abi;
  child->pgid = parent->pgid;
  child->sid = parent->sid;
  child->umask = parent->umask;
  sig_fork(&child->signals, &parent->signals);
  task_link_child(parent, child);

  scheduler_add_task(child);
  return (long)child->id.pid;
}

long sys_fork(registers_t *regs) { return fork_task(regs, 0, NULL, NULL); }

#define CLONE_VM 0x00000100
#define CLONE_SIGHAND 0x00000800
#define CLONE_VFORK 0x00004000
#define CLONE_THREAD 0x00010000
#define CLONE_SETTLS 0x00080000
#define CLONE_PARENT_SETTID 0x00100000
#define CLONE_CHILD_SETTID 0x01000000

/**
 * @brief clone(flags, newsp, parent_tid, child_tid, tls): a new process
 *
 * glibc's fork() is clone(SIGCHLD | CLONE_CHILD_SETTID | ...). Threads
 * (a shared address space that stays shared) are not supported yet.
 * CLONE_VM | CLONE_VFORK - posix_spawn(), system() - gets a copy instead
 * of the shared memory: the child only execs or exits, so all that is
 * lost is it reporting a failed exec through that memory. Other sharing
 * flags (CLONE_FILES, CLONE_FS) get copies too. CLONE_CHILD_CLEARTID only
 * matters for threads, and is ignored.
 */
long sys_clone(registers_t *regs) {
  uint64_t flags = regs->rdi;
  if (flags & (CLONE_THREAD | CLONE_SIGHAND | CLONE_SETTLS) ||
      (flags & CLONE_VM && !(flags & CLONE_VFORK)))
    return -ENOSYS;

  int *parent_tid = flags & CLONE_PARENT_SETTID ? (int *)regs->rdx : NULL;
  int *child_tid = flags & CLONE_CHILD_SETTID ? (int *)regs->r10 : NULL;
  if ((parent_tid && !user_int_ptr(parent_tid)) ||
      (child_tid && !user_int_ptr(child_tid)))
    return -EFAULT;
  return fork_task(regs, regs->rsi, parent_tid, child_tid);
}
