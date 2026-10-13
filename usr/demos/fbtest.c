/*
 * fbtest - exercise /dev/fb0 and /dev/input/events end to end.
 *
 * Reads the mode with FBIOGET_VSCREENINFO/FSCREENINFO, maps the
 * framebuffer, takes the display from the console (KD_GRAPHICS), draws a
 * test scene and runs a tiny paint program: the mouse moves a cursor,
 * the left button paints, Esc (or 60 s without input) quits.
 *
 * Built as a static musl binary (usr/Makefile), so it uses only the
 * Linux ABI the kernel implements.
 */
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>

#define CURSOR 12      /* cursor square, pixels */
#define BRUSH 6        /* paint dot, pixels */
#define IDLE_MS 60000  /* quit after this long without input */

static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;
static uint8_t *fb;    /* the mapped screen: uncached, write it in rows */
static uint8_t *back;  /* the scene in RAM; the cursor is never drawn here */
static int bytes_pp;

/* Pack 8-bit RGB into the framebuffer's pixel format. */
static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r >> (8 - var.red.length)) << var.red.offset) |
         ((uint32_t)(g >> (8 - var.green.length)) << var.green.offset) |
         ((uint32_t)(b >> (8 - var.blue.length)) << var.blue.offset);
}

static void put(uint8_t *buf, int x, int y, uint32_t px) {
  if (x < 0 || y < 0 || x >= (int)var.xres || y >= (int)var.yres)
    return;
  memcpy(buf + (size_t)y * fix.line_length + (size_t)x * bytes_pp, &px,
         bytes_pp); /* little endian: the low bytes are the pixel */
}

static void fill(uint8_t *buf, int x0, int y0, int w, int h, uint32_t px) {
  for (int y = y0; y < y0 + h; y++)
    for (int x = x0; x < x0 + w; x++)
      put(buf, x, y, px);
}

/* Copy a rectangle of the scene to the screen, one memcpy per row. */
static void present(int x0, int y0, int w, int h) {
  if (x0 < 0) { w += x0; x0 = 0; }
  if (y0 < 0) { h += y0; y0 = 0; }
  if (x0 + w > (int)var.xres) w = var.xres - x0;
  if (y0 + h > (int)var.yres) h = var.yres - y0;
  for (int y = y0; y < y0 + h; y++) {
    size_t off = (size_t)y * fix.line_length + (size_t)x0 * bytes_pp;
    memcpy(fb + off, back + off, (size_t)w * bytes_pp);
  }
}

static void draw_scene(void) {
  /* Vertical gradient background. */
  for (uint32_t y = 0; y < var.yres; y++) {
    uint8_t t = y * 255 / var.yres;
    fill(back, 0, y, var.xres, 1, rgb(20, 30 + t / 4, 60 + t / 2));
  }
  /* Colour bars: each channel's position and width must be right. */
  const uint32_t bars[] = {rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255),
                           rgb(255, 255, 255)};
  for (int i = 0; i < 4; i++)
    fill(back, 40 + i * 70, 40, 60, 60, bars[i]);

  /* A "window": title bar, body, close button. */
  int wx = var.xres / 2 - 200, wy = var.yres / 2 - 120;
  fill(back, wx + 6, wy + 6, 400, 240, rgb(0, 0, 0));       /* shadow */
  fill(back, wx, wy, 400, 240, rgb(230, 230, 230));
  fill(back, wx, wy, 400, 28, rgb(40, 90, 170));
  fill(back, wx + 372, wy + 4, 20, 20, rgb(200, 60, 60));
  fill(back, wx + 20, wy + 50, 360, 2, rgb(150, 150, 150));
}

static void draw_cursor(int x, int y) {
  fill(fb, x, y, CURSOR, CURSOR, rgb(255, 255, 255));
  fill(fb, x + 2, y + 2, CURSOR - 4, CURSOR - 4, rgb(0, 0, 0));
}

int main(void) {
  int fd = open("/dev/fb0", O_RDWR);
  if (fd < 0) { perror("open /dev/fb0"); return 1; }
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 ||
      ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
    perror("FBIOGET_*SCREENINFO");
    return 1;
  }
  printf("fb: %ux%u %ubpp pitch %u size %u, R%u:%u G%u:%u B%u:%u\n",
         var.xres, var.yres, var.bits_per_pixel, fix.line_length,
         fix.smem_len, var.red.offset, var.red.length, var.green.offset,
         var.green.length, var.blue.offset, var.blue.length);
  bytes_pp = (var.bits_per_pixel + 7) / 8;
  if (bytes_pp < 2 || bytes_pp > 4) {
    fprintf(stderr, "unsupported %u bpp\n", var.bits_per_pixel);
    return 1;
  }

  fb = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fb == MAP_FAILED) { perror("mmap /dev/fb0"); return 1; }
  back = calloc(1, (size_t)fix.line_length * var.yres);
  if (!back) { perror("calloc"); return 1; }

  int in = open("/dev/input/events", O_RDONLY);
  if (in < 0)
    perror("open /dev/input/events (no mouse, Esc unavailable)");

  printf("fbtest: mouse paints, Esc quits\n");
  fflush(stdout);
  /* From here on console output would draw over us. */
  if (ioctl(0, KDSETMODE, KD_GRAPHICS) < 0)
    perror("KDSETMODE (console may draw over the picture)");

  draw_scene();
  present(0, 0, var.xres, var.yres);
  int cx = var.xres / 2, cy = var.yres / 2;
  draw_cursor(cx, cy);

  int painting = 0, quit = 0;
  while (!quit) {
    struct pollfd pfd = {.fd = in, .events = POLLIN};
    if (in < 0 || poll(&pfd, 1, IDLE_MS) <= 0)
      break; /* no input device, or idle too long */

    struct input_event ev[32];
    ssize_t n = read(in, ev, sizeof(ev));
    int ox = cx, oy = cy;
    for (ssize_t i = 0; i < n / (ssize_t)sizeof(ev[0]); i++) {
      if (ev[i].type == EV_REL && ev[i].code == REL_X)
        cx += ev[i].value;
      else if (ev[i].type == EV_REL && ev[i].code == REL_Y)
        cy += ev[i].value;
      else if (ev[i].type == EV_KEY && ev[i].code == BTN_LEFT)
        painting = ev[i].value != 0;
      else if (ev[i].type == EV_KEY && ev[i].code == KEY_ESC && ev[i].value)
        quit = 1;
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= (int)var.xres) cx = var.xres - 1;
    if (cy >= (int)var.yres) cy = var.yres - 1;

    /* Erase the old cursor by restoring the scene under it. */
    present(ox, oy, CURSOR, CURSOR);
    if (painting) {
      fill(back, cx, cy, BRUSH, BRUSH, rgb(255, 200, 0));
      present(cx, cy, BRUSH, BRUSH);
    }
    draw_cursor(cx, cy);
  }

  /* Give the display back; the console redraws its text. Keys typed
   * while we ran also went to the tty: drop them, not to the shell. */
  ioctl(0, KDSETMODE, KD_TEXT);
  tcflush(0, TCIFLUSH);
  printf("fbtest: done\n");
  return 0;
}
