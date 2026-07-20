/*
 * System call dispatch table and handler.
 */

#include <hpet/hpet.h>
#include <hubble/printk.h>
#include <hubble/syscall.h>
#include <hubble/syscalls.h>
#include <msr.h>
#include <smp/scheduler.h>
#include <smp/task.h>

typedef long (*syscall_fn_t)(long, long, long, long, long, long);

static inline void wrfsbase(uint64_t base) {
  __asm__ volatile("wrfsbase %0" : : "r"(base) : "memory");
}

long arch_stub(long a1, long a2, long a3, long a4, long a5, long a6) {
  switch (a1) {
  case 0x1002:
    // printk("FS_BASE: %lx\n", a2);
    // task_t *current_task = get_current_task();
    // current_task->fs_base = a2;
    wrmsr(0xC0000100, (uint64_t)a2);
    // wrfsbase(a2);
    // uint64_t val = rdmsr(0xC0000100);
    // hpet_delay_ms(1000);
    // printk("FS_BASE: %lx\n", val);
    break;
  case 0x1003:
    return rdmsr(0xC0000100);
    break;
  default:
    break;
  }

  return 0;
}

long exit_stub(long a1) {
  task_exit(a1);
  __builtin_unreachable();
}

long sys_set_tid_address(int *tidptr) {
  //   task_t *current_task = get_current_task();
  //   current_task->clear_child_tid = tidptr; // optional
  return 1;
}

long sys_exit_group(int status) { return exit_stub(status); }
static syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_write] = (syscall_fn_t)sys_write,
    [SYS_read] = (syscall_fn_t)sys_read,
    [SYS_mmap] = (syscall_fn_t)sys_mmap,
    [SYS_open] = (syscall_fn_t)sys_open,
    [SYS_close] = (syscall_fn_t)sys_close,
    [SYS_spawn] = (syscall_fn_t)sys_spawn,
    [SYS_spawn_file] = (syscall_fn_t)sys_spawn_file,
    [SYS_lseek] = (syscall_fn_t)sys_lseek,
    [SYS_module_load] = (syscall_fn_t)sys_module_load,
    [SYS_module_unload] = (syscall_fn_t)sys_module_unload,
    [158] = (syscall_fn_t)arch_stub,
    [60] = (syscall_fn_t)exit_stub,
    [218] = (syscall_fn_t)sys_set_tid_address,
    [231] = (syscall_fn_t)sys_exit_group,
    [12] = (syscall_fn_t)sys_brk};
uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6) {
  if (num >= SYSCALL_COUNT || !syscall_table[num]) {
    printk(KERN_ERR "[SYSCALL] Invalid syscall: %d\n", num);
    return -1;
  }
  printk(KERN_DEBUG "[SYSCALL] %d(%d, %d, %d, %d, %d, %d)\n", num, a1, a2, a3,
         a4, a5, a6);

  return syscall_table[num](a1, a2, a3, a4, a5, a6);
}
