/**
 * @file fb.c
 * @brief Framebuffer VFS device — exposes linear framebuffer to user-space
 *
 * /dev/fb0 speaks the Linux fbdev ABI: FBIOGET_VSCREENINFO and
 * FBIOGET_FSCREENINFO describe the mode, mmap maps the pixels. The mode
 * is whatever the bootloader set; it cannot be changed.
 */
#include <fs/vfs/dev.h>
#include <higher_half.h>
#include <hubble/errno.h>
#include <hubble/module.h>
#include <hubble/platform.h>
#include <hubble/string.h>
#include <mm/pmm.h>

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

#define FB_TYPE_PACKED_PIXELS 0
#define FB_VISUAL_TRUECOLOR 2

/* Linux <linux/fb.h> layouts: user programs pass these exact structs. */
struct fb_bitfield {
  uint32_t offset;
  uint32_t length;
  uint32_t msb_right;
};

struct fb_var_screeninfo {
  uint32_t xres, yres;
  uint32_t xres_virtual, yres_virtual;
  uint32_t xoffset, yoffset;
  uint32_t bits_per_pixel;
  uint32_t grayscale;
  struct fb_bitfield red, green, blue, transp;
  uint32_t nonstd;
  uint32_t activate;
  uint32_t height, width; /* in mm, 0 = unknown */
  uint32_t accel_flags;
  uint32_t pixclock;
  uint32_t left_margin, right_margin, upper_margin, lower_margin;
  uint32_t hsync_len, vsync_len;
  uint32_t sync, vmode, rotate, colorspace;
  uint32_t reserved[4];
};
_Static_assert(sizeof(struct fb_var_screeninfo) == 160, "Linux fbdev ABI");

struct fb_fix_screeninfo {
  char id[16];
  uint64_t smem_start;
  uint32_t smem_len;
  uint32_t type;
  uint32_t type_aux;
  uint32_t visual;
  uint16_t xpanstep, ypanstep, ywrapstep;
  uint32_t line_length;
  uint64_t mmio_start;
  uint32_t mmio_len;
  uint32_t accel;
  uint16_t capabilities;
  uint16_t reserved[2];
};
_Static_assert(sizeof(struct fb_fix_screeninfo) == 80, "Linux fbdev ABI");

/* Bytes userspace may map: whole pages, since mmap works in pages. */
static uint64_t fb_size(void) {
  uint64_t bytes = (uint64_t)g_platform.fb_pitch * g_platform.fb_height;
  return (bytes + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
}

/**
 * @brief Physical address of @p offset in the framebuffer
 *
 * fb_base is Limine's HHDM (virtual) address; mmap needs the physical
 * one. A range reaching past the framebuffer returns 0, which fails the
 * mmap: it would otherwise expose whatever physical memory follows.
 */
static uint64_t fb_mmap(uint64_t offset, size_t size) {
  if (!g_platform.fb_base || offset > fb_size() ||
      size > fb_size() - offset)
    return 0;
  return virt_to_phys(g_platform.fb_base) + offset;
}

static void fb_get_var(struct fb_var_screeninfo *var) {
  memset(var, 0, sizeof(*var));
  var->xres = var->xres_virtual = g_platform.fb_width;
  var->yres = var->yres_virtual = g_platform.fb_height;
  var->bits_per_pixel = g_platform.fb_bpp;
  var->red = (struct fb_bitfield){g_platform.fb_red_shift,
                                  g_platform.fb_red_size, 0};
  var->green = (struct fb_bitfield){g_platform.fb_green_shift,
                                    g_platform.fb_green_size, 0};
  var->blue = (struct fb_bitfield){g_platform.fb_blue_shift,
                                   g_platform.fb_blue_size, 0};
}

static void fb_get_fix(struct fb_fix_screeninfo *fix) {
  memset(fix, 0, sizeof(*fix));
  strncpy(fix->id, "hubblefb", sizeof(fix->id) - 1);
  fix->smem_start = virt_to_phys(g_platform.fb_base);
  fix->smem_len = (uint32_t)fb_size();
  fix->type = FB_TYPE_PACKED_PIXELS;
  fix->visual = FB_VISUAL_TRUECOLOR;
  fix->line_length = g_platform.fb_pitch;
}

/* TODO: arg is an unchecked user pointer, as in every ioctl so far. */
static long fb_vfs_ioctl(unsigned long request, void *arg) {
  switch (request) {
  case FBIOGET_VSCREENINFO:
    if (!arg)
      return -EFAULT;
    fb_get_var(arg);
    return 0;

  case FBIOGET_FSCREENINFO:
    if (!arg)
      return -EFAULT;
    fb_get_fix(arg);
    return 0;

  case FBIOPUT_VSCREENINFO:
    return -EINVAL; /* the bootloader's mode is the only mode */

  default:
    return -ENOTTY;
  }
}

static int fb_init(void) {
  if (!g_platform.fb_base)
    return -ENODEV;
  dev_vfs_register("fb0", fb_mmap, NULL, NULL);
  dev_vfs_set_char_ops("fb0", NULL, fb_vfs_ioctl, NULL, NULL);
  return 0;
}

module_init(fb_init);
MODULE_NAME("fb");
