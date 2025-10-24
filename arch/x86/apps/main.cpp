#include "utils/framebuffer.h"
#include "utils/color.h"

#include "screen/screen.h"
#include "window/window.h"
#include "terminal/terminal.h"

#include "utils/bwfvideo.h"

#include <stdio.h>
#include <stdlib.h>

extern "C" void os_main(BootInfo *bi)
{
  Screen screen(bi->framebuffer);
  screen.clearScreen();

  Window win(screen, 10, 10, screen.getWidth() - 20, screen.getHeight() - 20, rgb(50, 50, 50));
  win.clear();

  Terminal term(win);
  term.init();

  term.run();
  // Window win(screen, 10, 10, 400, 300, rgba(50, 50, 50, 0));
  // win.clear();

  // play_bwvid(bi->framebuffer, "/output.bwv", bi->framebuffer->width, bi->framebuffer->bpp, win.getX(), win.getY());
}
