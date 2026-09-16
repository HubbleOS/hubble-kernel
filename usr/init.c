/*
 * init - Minimal /init for standalone hubble-kernel testing.
 *
 * Proves the full boot chain works:
 *   Limine -> kernel.elf -> initramfs.img -> /init
 *
 * This is a standalone userspace binary with no dependencies.
 * It uses direct syscalls and loops forever to keep the kernel alive.
 */

static long sys_write(int fd, const void *buf, unsigned long len) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(1 /* SYS_write */), "D"(fd), "S"(buf), "d"(len)
                   : "rcx", "r11", "memory");
  return ret;
}

static void sys_halt(void) {
  while (1) {
    __asm__ volatile("pause");
  }
}

static unsigned long strlen(const char *s) {
  unsigned long n = 0;
  while (s[n])
    n++;
  return n;
}

static void puts(const char *s) { sys_write(1, s, strlen(s)); }

void _start(void) {
  puts("\n");
  puts("===================================\n");
  puts("  Hubble Kernel (minimal init)\n");
  puts("===================================\n");
  puts("\n");
  puts("  Boot chain verified:\n");
  puts("    Limine -> kernel -> initramfs -> /init\n");
  puts("\n");

  sys_halt();
}
