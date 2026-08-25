#pragma once

#include <stddef.h>

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_stat 4
#define SYS_spawn 6
#define SYS_spawn_file 7
#define SYS_lseek 8
#define SYS_mmap 9
#define SYS_module_load 10

#define SYSCALL_COUNT 256

// Using SYSCALL instruction (faster)
static inline long do_syscall_fast(long num, long arg1, long arg2, long arg3,
                                   long arg4, long arg5, long arg6) {
  long ret;
  register long r10 __asm__("r10") = arg4;
  register long r8 __asm__("r8") = arg5;
  register long r9 __asm__("r9") = arg6;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10),
                     "r"(r8), "r"(r9)
                   : "rcx", "r11", "memory");
  return ret;
}

// We use INT 0x80 (compatibility)
static inline long do_syscall_int(long num, long arg1, long arg2, long arg3,
                                  long arg4, long arg5, long arg6) {
  long ret;
  __asm__ volatile("movq %1, %%rax\n"
                   "movq %2, %%rdi\n"
                   "movq %3, %%rsi\n"
                   "movq %4, %%rdx\n"
                   "movq %5, %%r10\n"
                   "movq %6, %%r8\n"
                   "movq %7, %%r9\n"
                   "int $0x80\n"
                   "movq %%rax, %0\n"
                   : "=r"(ret)
                   : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4),
                     "r"(arg5), "r"(arg6)
                   : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "memory");
  return ret;
}

#define do_syscall do_syscall_fast
// #define do_syscall do_syscall_int

#define syscall1(n, a1) do_syscall(n, a1, 0, 0, 0, 0, 0)
#define syscall2(n, a1, a2) do_syscall(n, a1, a2, 0, 0, 0, 0)
#define syscall3(n, a1, a2, a3) do_syscall(n, a1, a2, a3, 0, 0, 0)
#define syscall4(n, a1, a2, a3, a4) do_syscall(n, a1, a2, a3, a4, 0, 0)
#define syscall5(n, a1, a2, a3, a4, a5) do_syscall(n, a1, a2, a3, a4, a5, 0)
#define syscall6(n, a1, a2, a3, a4, a5, a6)                                    \
  do_syscall(n, a1, a2, a3, a4, a5, a6)

typedef long (*syscall_fn_t)(long arg1, long arg2, long arg3, long arg4,
                             long arg5, long arg6);
