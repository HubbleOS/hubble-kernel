#include "utils/framebuffer.h"
#include "utils/color.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

// #include <iso646.h>

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

  // handle_neofetch();

  int r, g, b;
  while (1)
  {
    printf("Enter red value:");
    scanf("%d", &r);
    printf("Enter green value:");
    scanf("%d", &g);
    printf("Enter blue value:");
    scanf("%d", &b);
    if (r < 0 or r > 255 or g < 0 or g > 255 or b < 0 or b > 255)
    {
      printf("Invalid color values. Please enter values between 0 and 255.\n");
      continue;
    }
    uint32_t color = rgb(r, g, b);
    screen.clearScreen();
    for (int y = 0; y < screen.getHeight(); y++)
    {
      for (int x = 0; x < screen.getWidth(); x++)
      {
        screen.drawPixel(x, y, color);
      }
    }
    printf("Screen cleared with color RGB(%d, %d, %d).\n", r, g, b);
  }

  // handle_neofetch();
  // putchar('\n');

  // void *a = malloc(32);
  // void *b = malloc(32);
  // void *c = malloc(32);

  // printf("%p\n", a);
  // printf("%p\n", b);
  // printf("%p\n", c);
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
