#pragma once

/**
 * @brief System call numbers.
 */

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_stat 4
#define SYS_fstat 5
#define SYS_lstat 6
#define SYS_poll 7
#define SYS_lseek 8
#define SYS_mmap 9
#define SYS_munmap 11
#define SYS_brk 12
#define SYS_rt_sigaction 13
#define SYS_rt_sigprocmask 14
#define SYS_ioctl 16
#define SYS_readv 19
#define SYS_writev 20
#define SYS_access 21
#define SYS_nanosleep 35
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_fcntl 72
#define SYS_fsync 74
#define SYS_fdatasync 75
#define SYS_truncate 76
#define SYS_ftruncate 77
#define SYS_rename 82
#define SYS_mkdir 83
#define SYS_rmdir 84
#define SYS_unlink 87
#define SYS_gettimeofday 96
#define SYS_getdents64 217
#define SYS_clock_gettime 228
#define SYS_openat 257
#define SYS_getcwd 79
#define SYS_pipe 22
#define SYS_dup 32
#define SYS_dup2 33
#define SYS_getpid 39
#define SYS_dup3 292
#define SYS_pipe2 293
#define SYS_getppid 110
#define SYS_gettid 186
#define SYS_getuid 107
#define SYS_arch_prctl 158
#define SYS_set_tid_address 218
#define SYS_exit_group 231
/* Kernel-private calls. Numbers 0-499 follow the Linux x86_64 ABI, which
 * static libc binaries (busybox) rely on; private calls live above it so
 * they can never shadow a real syscall (spawn used to sit on lstat). */
#define SYS_spawn 500
#define SYS_spawn_file 501
#define SYS_module_load 502
#define SYS_module_unload 503

#define SYSCALL_COUNT 512
