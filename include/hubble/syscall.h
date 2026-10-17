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
#define SYS_uname 63
#define SYS_madvise 28
#define SYS_setsockopt 54
#define SYS_getsockopt 55
#define SYS_getrlimit 97
#define SYS_setrlimit 160
#define SYS_prctl 157
#define SYS_prlimit64 302
#define SYS_setpgid 109
#define SYS_getpgrp 111
#define SYS_setsid 112
#define SYS_getpgid 121
#define SYS_getsid 124
#define SYS_socket 41
#define SYS_connect 42
#define SYS_accept 43
#define SYS_sendto 44
#define SYS_recvfrom 45
#define SYS_sendmsg 46
#define SYS_recvmsg 47
#define SYS_shutdown 48
#define SYS_bind 49
#define SYS_listen 50
#define SYS_socketpair 53
#define SYS_accept4 288
#define SYS_memfd_create 319
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
#define SYS_chdir 80
#define SYS_pipe 22
#define SYS_dup 32
#define SYS_dup2 33
#define SYS_getpid 39
#define SYS_dup3 292
#define SYS_pipe2 293
#define SYS_getppid 110
#define SYS_gettid 186
#define SYS_getuid 102
#define SYS_getgid 104
#define SYS_geteuid 107
#define SYS_getegid 108
#define SYS_readlink 89
#define SYS_chmod 90
#define SYS_fchmod 91
#define SYS_umask 95
#define SYS_flock 73
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
