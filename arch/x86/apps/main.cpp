#include <bootinfo/framebuffer.h>
#include <utils/color.h>
#include "screen/screen.h"
#include "window/window.h"
#include "terminal/terminal.h"
#include <fs/vfs/vfs.h>
#include "printk.h"

#include <sys/syscall.h>
#include <syscalls/syscall.h>

#include "mm/slab.h"

#include <gdt/interrupt.h>

extern "C" void os_main(BootInfo *bi)
{
  Screen screen(bi->framebuffer);
  screen.clearScreen();

  Window win(screen, 10, 10, screen.getWidth() - 20, screen.getHeight() - 20, rgb(50, 50, 50));
  win.clear();

  Terminal term(win);
  term.init();

  term.run();
}