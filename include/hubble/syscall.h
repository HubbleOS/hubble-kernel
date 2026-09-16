#pragma once

/**
 * @brief System call numbers.
 */

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_stat 4
#define SYS_lseek 8
#define SYS_spawn 6
#define SYS_spawn_file 7
#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_brk 12
#define SYS_rt_sigaction 13
#define SYS_rt_sigprocmask 14
#define SYS_readv 19
#define SYS_writev 20
#define SYS_module_load 255
#define SYS_module_unload 255
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_getcwd 79
#define SYS_getuid 107
#define SYS_arch_prctl 158
#define SYS_set_tid_address 218
#define SYS_exit_group 231
#define SYSCALL_COUNT 256
