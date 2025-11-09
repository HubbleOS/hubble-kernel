#include <bootinfo/framebuffer.h>
#include <utils/color.h>
#include "screen/screen.h"
#include "window/window.h"
#include "terminal/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <fs/vfs/vfs.h>
#include "printk.h"

#include <sys/syscall.h>
#include <syscalls/syscall.h>

extern "C" int load_elf_and_run(const char *path);

void test_syscall(void)
{
  uint64_t result;

  printk("Test 1: int $0x80 syscall(0)\n");
  asm volatile(
      "movq $0, %%rax\n"
      "int $0x80\n"
      "movq %%rax, %0"
      : "=r"(result)
      :
      : "rax");
  printk("  Result: %lu\n", result);

  // Тест SYSCALL
  printk("Test 2: syscall instruction syscall(0)\n");
  asm volatile(
      "movq $0, %%rax\n"
      "syscall\n"
      "movq %%rax, %0"
      : "=r"(result)
      :
      : "rax", "rcx", "r11");
  printk("  Result: %lu\n", result);
}

extern "C" void os_main(BootInfo *bi)
{
  Screen screen(bi->framebuffer);
  screen.clearScreen();

  Window win(screen, 10, 10, screen.getWidth() - 20, screen.getHeight() - 20, rgb(50, 50, 50));
  win.clear();

  Terminal term(win);
  term.init();

  // test_syscall();

  // int res = load_elf_and_run("/usr/bin/user.elf");
  // if (res != 0)
  // {
  //   term.print("Failed to load user.elf\n");
  //   return;
  // }

  term.run();
}