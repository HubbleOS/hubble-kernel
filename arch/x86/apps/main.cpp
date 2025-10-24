#include "utils/framebuffer.h"
#include "utils/color.h"

#include "screen/screen.h"
#include "window/window.h"
#include "terminal/terminal.h"

#include <stdio.h>
#include <stdlib.h>

#include <fs/fat32/fat.h>
#include <fs/fat32/fat_structs.h>
#include <fs/ata/ata.h>
#include <fs/gpt/gpt.h>
#include <fs/gpt/gpt_struct.h>
#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <fs/nvme/nvme.h>
#include <fs/pci/pci.h>

extern "C" void os_main(BootInfo *bi)
{
  Screen screen(bi->framebuffer);
  screen.clearScreen();

  Window win(screen, 10, 10, screen.getWidth() - 20, screen.getHeight() - 20, rgb(50, 50, 50));
  win.clear();

  Terminal term(win);
  term.init();

  char buffer[1024];

  VFS_File *f = vfs_open("/tesit.txt", VFS_O_CREAT | VFS_O_RDWR);
  // vfs_write(f, "Hello wo123", 11);
  vfs_lseek(f, 0, SEEK_SET);
  printf("Reading file: ");
  vfs_read(f, buffer, 1024);
  printf("File content: ");
  for (int i = 0; i < 1024; i++)
  {
    if (buffer[i] == '\0')
      break;
    printf("%c", buffer[i]);
  }

  // term.run();
}
