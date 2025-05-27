#include "utils/framebuffer.h"
#include "utils/color.h"
#include <stdio.h>
#include <stdlib.h>

void handle_neofetch();

class Screen
{
private:
  framebuffer_info_t *fb;

public:
  Screen(framebuffer_info_t *fb) : fb(fb) {}
  ~Screen() {}

  void drawPixel(int x, int y, uint32_t color)
  {
    auto pixel_ptr = (uint32_t *)fb->base;
    pixel_ptr[y * fb->width + x] = color;
  }

  int getWidth() const { return fb->width; }
  int getHeight() const { return fb->height; }
  void clearScreen()
  {
    for (int i = 0; i < fb->width * fb->height; i++)
      drawPixel(i % fb->width, i / fb->width, rgb(0, 0, 0));
  }
};

extern "C" void os_main(framebuffer_info_t *fb)
{
  Screen screen(fb);
  screen.clearScreen();

  handle_neofetch();
  putchar('\n');

  void *a = malloc(32);
  void *b = malloc(32);
  void *c = malloc(32);

  printf("%p\n", a);
  printf("%p\n", b);
  printf("%p\n", c);
}

void handle_neofetch()
{
  printf(" _    _         _      _      _        \n"
         "| |  | |       | |    | |    | |       \n"
         "| |__| | _   _ | |__  | |__  | |  ___  \n"
         "|  __  || | | || '_ \\ | '_ \\ | | / _ \\ \n"
         "| |  | || |_| || |_) || |_) || ||  __/ \n"
         "|_|  |_| \\__,_||_.__/ |_.__/ |_| \\___| \n");
}
