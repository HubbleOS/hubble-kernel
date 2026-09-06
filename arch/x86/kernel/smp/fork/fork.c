/*
 * Syscall: fork() - create a copy-on-write clone of the calling process.
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
#include <smp/scheduler.h>
#include <smp/task.h>

/**
 * @brief Handle the fork() syscall
 *
 * @param regs Raw register frame captured at the `syscall` instruction
 * @return Child PID to the parent (the child itself resumes with 0 in RAX,
 *         set directly in its cloned context rather than returned here)
 */
long sys_fork(registers_t *regs) {
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

  /* `syscall` loads the return RIP into RCX and RFLAGS into R11 - both are
   * saved as plain GPRs by syscall_entry.asm, so they're sitting right
   * here in the trap frame. */
  child->exec.context.rip = regs->rcx;
  child->exec.context.rflags = regs->r11;

  /* The user RSP at syscall time isn't in the trap frame at all (SYSCALL
   * doesn't save it) - it's stashed in the same per-CPU slot schedule()
   * reads to preempt a task mid-syscall. */
  uint64_t user_rsp;
  asm volatile("mov %%gs:8, %0" : "=r"(user_rsp));
  child->exec.context.rsp = user_rsp;

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
  child->mm.page_table = vmm_fork_pagemap(parent->mm.vm_map);
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

  /* File descriptors: shared by value (this kernel has no per-fd
   * refcounting yet, so this is exactly as sound - and as limited - as
   * the rest of the current fd handling). */
  child->fdtable = parent->fdtable;

  child->linkage.parent = parent;
  child->linkage.sibling = parent->linkage.children;
  parent->linkage.children = child;

  scheduler_add_task(child);
  return (long)child->id.pid;
}
