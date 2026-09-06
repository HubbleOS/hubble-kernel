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
#define SYS_mmap 9
#define SYS_spawn 6
#define SYS_spawn_file 7
#define SYS_module_load 255
#define SYS_module_unload 255
#define SYS_munmap 11
#define SYS_fork 57
#define SYS_execve 59
#define SYS_wait4 61
#define SYSCALL_COUNT 256
