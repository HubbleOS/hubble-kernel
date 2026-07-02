/**
 * @file bwfvideo.c
 * @brief Black-and-white frame video player
 *
 * Reads a custom BW-format video file from the VFS and renders
 * each frame to the framebuffer at a hard-coded ~30 FPS.
 */

#include "bwfvideo.h"
#include <hubble/color.h>
#include <hubble/printk.h>
#include <stdint.h>

#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>

#include <mm/kmalloc.h>

/* -- Frame Header ---------------------------------------------- */

/**
 * @brief On-disk header for a single BW-format frame
 */
struct BWFrameHeader {
  uint16_t width;
  uint16_t height;
  uint32_t size;
} __attribute__((packed));

/* -- Helpers (static) ------------------------------------------ */

/**
 * @brief Busy-wait delay in milliseconds
 *
 * @param ms Milliseconds to wait
 */
void sleep_ms(uint32_t ms) {
  for (volatile uint64_t i = 0; i < (ms * 100000); i++) {
    __asm__ __volatile__("nop");
  }
}

/**
 * @brief Set one pixel on the framebuffer
 *
 * @param bi       Framebuffer info
 * @param x        X position
 * @param y        Y position
 * @param color    Pixel colour
 * @param fb_pitch Framebuffer pitch in bytes
 * @param bpp      Bits per pixel
 */
static inline void putpixel(framebuffer_info_t *bi, int x, int y, color_t color,
                            uint32_t fb_pitch, uint32_t bpp) {
  uint8_t *ptr = bi->base + y * fb_pitch + x * (bpp / 8);
  *(color_t *)ptr = color;
}

/* -- Frame Rendering ------------------------------------------- */

/**
 * @brief Decode and draw a single BW frame
 *
 * @param bi      Framebuffer info
 * @param data    Raw frame data (header + packed pixels)
 * @param x_start Screen X offset
 * @param y_start Screen Y offset
 */
void draw_frame(framebuffer_info_t *bi, uint8_t *data, int x_start,
                int y_start) {
  struct BWFrameHeader *hdr = (struct BWFrameHeader *)data;
  uint8_t *pixels = data + sizeof(struct BWFrameHeader);

  int w = hdr->width;
  int h = hdr->height;
  int row_bytes = (w + 7) / 8;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int byte_index = y * row_bytes + x / 8;
      int bit_index = 7 - (x % 8);
      int bit = (pixels[byte_index] >> bit_index) & 1;
      color_t color = bit ? COLOR_WHITE : COLOR_BLACK;

      int px = x_start + x;
      int py = y_start + y;
      if (px >= 0 && px < bi->width && py >= 0 && py < bi->height)
        putpixel(bi, px, py, color, bi->pitch, bi->bpp);
    }
  }
}

/* -- Video Playback -------------------------------------------- */

/**
 * @brief Play a BW-format video file at the given screen position
 *
 * Opens the file, reads frames sequentially, renders them, and
 * sleeps ~33 ms between frames.
 *
 * @param bi   Framebuffer info
 * @param path VFS path to the video file
 * @param x    Screen X offset
 * @param y    Screen Y offset
 */
void play_bwvid(framebuffer_info_t *bi, const char *path, int x, int y) {
  VFS_File *file = vfs_open(path, VFS_O_RDONLY);
  if (IS_ERR(file)) {
    printk(KERN_ERR "Failed to open file %s\n", path);
    return;
  }

  while (1) {
    struct BWFrameHeader hdr;

    int r = vfs_read(file, &hdr, sizeof(hdr));
    if (r != sizeof(hdr))
      break;

    uint8_t *frame_data = kmalloc(sizeof(hdr) + hdr.size, GFP_KERNEL);
    if (!frame_data)
      break;

    *(struct BWFrameHeader *)frame_data = hdr;

    r = vfs_read(file, frame_data + sizeof(hdr), hdr.size);
    if (r != (int)hdr.size) {
      kfree(frame_data);
      printk(KERN_ERR "frame read error %d, expected %d\n", r, hdr.size);
      break;
    }
    printk(KERN_INFO "frame size: %d\n", hdr.size);
    printk(KERN_INFO "Frame %dx%d, size=%d\n", hdr.width, hdr.height, hdr.size);

    draw_frame(bi, frame_data, x, y);

    kfree(frame_data);
    sleep_ms(33);
  }
}
