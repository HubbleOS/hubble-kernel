/*
 * System call dispatch table and handler.
 */

#include <hpet/hpet.h>
#include <hubble/errno.h>
#include <hubble/printk.h>
#include <hubble/string.h>
#include <hubble/syscall.h>
#include <hubble/syscalls.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <msr.h>
#include <smp/scheduler.h>
#include <smp/task.h>

typedef long (*syscall_fn_t)(long, long, long, long, long, long);

long sys_set_tid_address(int *tidptr) {
  // task_t *current_task = get_current_task();
  //   current_task->clear_child_tid = tidptr; // optional
  return 1;
}

long sys_rt_sigaction(int sig, const void *act, void *oldact,
                      size_t sigsetsize) {
  return 0; // прийняли, ігноруємо
}
long sys_rt_sigprocmask(int how, const void *set, void *oldset,
                        size_t sigsetsize) {
  return 0;
}

long sys_exit_group(int status) { return exit_stub(status); }
static syscall_fn_t syscall_table[SYSCALL_COUNT] = {
    [SYS_write] = (syscall_fn_t)sys_write,
    [SYS_read] = (syscall_fn_t)sys_read,
    [SYS_mmap] = (syscall_fn_t)sys_mmap,
    [SYS_munmap] = (syscall_fn_t)sys_munmap,
    [SYS_open] = (syscall_fn_t)sys_open,
    [SYS_stat] = (syscall_fn_t)sys_stat,
    [SYS_close] = (syscall_fn_t)sys_close,
    [SYS_spawn] = (syscall_fn_t)sys_spawn,
    [SYS_spawn_file] = (syscall_fn_t)sys_spawn_file,
    [SYS_lseek] = (syscall_fn_t)sys_lseek,
    [SYS_module_load] = (syscall_fn_t)sys_module_load,
    [SYS_module_unload] = (syscall_fn_t)sys_module_unload,
    [SYS_arch_prctl] = (syscall_fn_t)arch_prctl,
    [SYS_exit] = (syscall_fn_t)exit_stub,
    [SYS_set_tid_address] = (syscall_fn_t)sys_set_tid_address,
    [SYS_exit_group] = (syscall_fn_t)sys_exit_group,
    [SYS_brk] = (syscall_fn_t)sys_brk,
    [SYS_writev] = (syscall_fn_t)sys_writev,
    [SYS_readv] = (syscall_fn_t)sys_readv,
    [SYS_rt_sigaction] = (syscall_fn_t)sys_rt_sigaction,
    [SYS_rt_sigprocmask] = (syscall_fn_t)sys_rt_sigprocmask,
    [SYS_getcwd] = (syscall_fn_t)sys_getcwd,
    [SYS_getuid] = (syscall_fn_t)sys_getuid,
    [SYS_wait4] = (syscall_fn_t)sys_wait4,
};

uint64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3,
                         uint64_t a4, uint64_t a5, uint64_t a6) {
  if (num >= SYSCALL_COUNT || !syscall_table[num]) {
    printk(KERN_ERR "[SYSCALL] Invalid syscall: %d\n", num);
    return -ENOSYS;
  }
  uint64_t ret = syscall_table[num](a1, a2, a3, a4, a5, a6);
  printk(KERN_DEBUG "[SYSCALL] %d(%d, %d, %d, %d, %d, %d) --> %ld\n", num, a1,
         a2, a3, a4, a5, a6, ret);
  return ret;
}
