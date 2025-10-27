#include <bootinfo/framebuffer.h>
#include <utils/color.h>
#include "screen/screen.h"
#include "window/window.h"
#include "terminal/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <fs/vfs/vfs.h>
#include <elf/elf.h>
#include "printk.h"

// Helper macros for error pointer handling
#define IS_ERR(ptr) ((uintptr_t)(ptr) >= (uintptr_t)-4095)
#define PTR_ERR(ptr) ((long)(ptr))

extern "C" void load_and_run_elf(const char *path);

extern "C" void os_main(BootInfo *bi)
{
  Screen screen(bi->framebuffer);
  screen.clearScreen();

  Window win(screen, 10, 10, screen.getWidth() - 20, screen.getHeight() - 20, rgb(50, 50, 50));
  win.clear();

  Terminal term(win);
  term.init();

  const char *path = "/usr/bin/hello";

  // ПЕРВЫМ ДЕЛОМ: Проверяем что файловая система работает
  printk("\n========================================\n");
  printk("=== File System Debug ===\n");
  printk("========================================\n\n");

  printk("1. Listing root directory:\n");
  Directory dir = vfs_readdir("/");
  for (int i = 0; i < dir.count; i++)
  {
    printk("   %s %s\n", dir.entries[i].name,
           dir.entries[i].is_dir ? "(dir)" : "(file)");
  }
  printk("\n");

  // Проверяем /usr
  printk("2. Listing /usr directory:\n");
  Directory usr_dir = vfs_readdir("/usr");
  for (int i = 0; i < usr_dir.count; i++)
  {
    printk("   %s %s\n", usr_dir.entries[i].name,
           usr_dir.entries[i].is_dir ? "(dir)" : "(file)");
  }
  printk("\n");

  // Проверяем /usr/bin
  printk("3. Listing /usr/bin directory:\n");
  Directory bin_dir = vfs_readdir("/usr/bin");
  for (int i = 0; i < bin_dir.count; i++)
  {
    printk("   %s %s\n", bin_dir.entries[i].name,
           bin_dir.entries[i].is_dir ? "(dir)" : "(file)");
  }
  printk("\n");

  // Теперь пробуем открыть и прочитать файл
  printk("4. Testing file read: %s\n", path);
  VFS_File *test = vfs_open(path, VFS_O_RDONLY);

  if (!test || IS_ERR(test))
  {
    printk("   ❌ Failed to open file (error: %ld)\n",
           IS_ERR(test) ? PTR_ERR(test) : -1);
    printk("\n========================================\n");
    printk("File system test FAILED!\n");
    printk("Cannot proceed with ELF loading.\n");
    printk("========================================\n\n");

    term.run(); // Запускаем терминал для дебага
    return;
  }

  printk("   ✓ File opened successfully\n");
  printk("   File size: %u bytes\n", test->node->size);

  // Читаем первые 128 байт
  uint8_t buf[128];
  int n = vfs_read(test, buf, 128);

  printk("   Read %d bytes\n", n);

  if (n < 64)
  {
    printk("   ❌ Failed to read enough data (got %d bytes, expected 128)\n", n);
    printk("\n========================================\n");
    printk("File system test FAILED!\n");
    printk("Cannot proceed with ELF loading.\n");
    printk("========================================\n\n");

    term.run();
    return;
  }

  // Показываем первые 64 байта
  printk("   First 64 bytes:\n");
  for (int i = 0; i < 64 && i < n; i++)
  {
    if (i % 16 == 0)
      printk("   %04x: ", i);
    printk("%02x ", buf[i]);
    if ((i + 1) % 16 == 0)
      printk("\n");
  }
  printk("\n");

  // Проверяем ELF magic
  if (buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F')
  {
    printk("   ✓ Valid ELF magic found!\n");
    printk("   ✓ ELF class: %s\n", buf[4] == 2 ? "64-bit" : "32-bit");
    printk("   ✓ ELF data: %s\n", buf[5] == 1 ? "little-endian" : "big-endian");
  }
  else
  {
    printk("   ❌ Invalid ELF magic!\n");
    printk("   Expected: 7F 45 4C 46 (0x7F 'E' 'L' 'F')\n");
    printk("   Got:      %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3]);
    printk("\n========================================\n");
    printk("File system test FAILED!\n");
    printk("File exists but is not a valid ELF!\n");
    printk("========================================\n\n");

    term.run();
    return;
  }

  printk("\n========================================\n");
  printk("✓ File system test PASSED!\n");
  printk("✓ ELF file is valid and readable\n");
  printk("========================================\n\n");

  // Теперь пробуем загрузить ELF
  printk("Press any key to load ELF...\n");
  // Можно добавить ожидание клавиши если есть

  printk("\n========================================\n");
  printk("Starting ELF loader...\n");
  printk("========================================\n\n");

  load_and_run_elf(path);

  // Если мы сюда вернулись - что-то пошло не так
  printk("\n========================================\n");
  printk("⚠️  Returned from load_and_run_elf()\n");
  printk("This should not happen!\n");
  printk("========================================\n\n");

  term.run();
}