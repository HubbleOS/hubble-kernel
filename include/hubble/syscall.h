#pragma once

/**
 * @brief System call numbers.
 */

#define SYS_write         1
#define SYS_read          2
#define SYS_mmap          3
#define SYS_open          4
#define SYS_close         5
#define SYS_spawn         6
#define SYS_spawn_file    7
#define SYS_lseek         8
#define SYS_module_load   9
#define SYS_module_unload 10

#define SYSCALL_COUNT 256
