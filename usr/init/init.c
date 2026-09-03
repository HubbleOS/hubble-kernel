/*
 * init - First userspace process for Hubble OS.
 *
 * Minimal /init that proves the full boot chain works:
 *   Limine -> kernel.elf -> initramfs.img -> /init
 */

#include <stdint.h>

#define SYS_write 1

static long sys_write(int fd, const void *buf, unsigned long len) {
    long ret;
    __asm__ volatile("syscall"
        : "=a"(ret)
        : "a"(SYS_write), "D"(fd), "S"(buf), "d"(len)
        : "rcx", "r11", "memory");
    return ret;
}

static unsigned long strlen(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

static void puts(const char *s) {
    sys_write(1, s, strlen(s));
}

void _start(void) {
    puts("\n");
    puts("===================================\n");
    puts("  Hubble OS  /init started\n");
    puts("===================================\n");
    puts("\n");
    puts("  Boot chain verified:\n");
    puts("    Limine -> kernel -> initramfs -> /init\n");
    puts("\n");

    while (1) {
        __asm__ volatile("pause");
    }
}
