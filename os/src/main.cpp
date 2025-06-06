#include "utils/color.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

void handle_neofetch();

extern "C" void os_main()
{
  handle_neofetch();

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

    printf("Screen cleared with color RGB(%d, %d, %d).\n", r, g, b);
  }
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
