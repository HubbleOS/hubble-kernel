/**
 * @file main.c
 * @brief Bootloader main entry — page table setup, ELF loading, kernel jump
 */

#include <asm.h>
#include <bootinfo/bootinfo.h>
#include <elf.h>
#include <loader_context.h>
#include <stddef.h>
#include <stdint.h>

#include "higher_half.h"

static fb_info_t fb_g;

typedef struct {
  uint64_t fb_base;
  uint32_t fb_width;
  uint32_t fb_height;
  uint32_t fb_pitch;
} platform_info_t;

platform_info_t g_platform;

#include <hubble/font.h>

const uint8_t font[256][8] = {
    [0 ... 31] = {0},

    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['!'] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00},
    ['"'] = {0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['#'] = {0x24, 0x24, 0x7E, 0x24, 0x7E, 0x24, 0x24, 0x00},
    ['$'] = {0x10, 0x3E, 0x50, 0x3C, 0x12, 0x7C, 0x10, 0x00},
    ['%'] = {0x62, 0x64, 0x08, 0x10, 0x26, 0x46, 0x00, 0x00},
    ['&'] = {0x30, 0x48, 0x50, 0x20, 0x52, 0x4C, 0x32, 0x00},
    ['\''] = {0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['('] = {0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00},
    [')'] = {0x20, 0x10, 0x08, 0x08, 0x08, 0x10, 0x20, 0x00},
    ['*'] = {0x00, 0x24, 0x18, 0x7E, 0x18, 0x24, 0x00, 0x00},
    ['+'] = {0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x00},
    [','] = {0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x10, 0x20},
    ['-'] = {0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00},
    ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['/'] = {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00, 0x00},

    ['0'] = {0x3C, 0x46, 0x4A, 0x52, 0x62, 0x46, 0x3C, 0x00},
    ['1'] = {0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x7C, 0x00},
    ['2'] = {0x3C, 0x42, 0x02, 0x1C, 0x20, 0x40, 0x7E, 0x00},
    ['3'] = {0x3C, 0x42, 0x02, 0x1C, 0x02, 0x42, 0x3C, 0x00},
    ['4'] = {0x08, 0x18, 0x28, 0x48, 0x7E, 0x08, 0x08, 0x00},
    ['5'] = {0x7E, 0x40, 0x7C, 0x02, 0x02, 0x42, 0x3C, 0x00},
    ['6'] = {0x1C, 0x20, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00},
    ['7'] = {0x7E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10, 0x00},
    ['8'] = {0x3C, 0x42, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00},
    ['9'] = {0x3C, 0x42, 0x42, 0x3E, 0x02, 0x04, 0x38, 0x00},

    [':'] = {0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00},
    [';'] = {0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x10, 0x20},
    ['<'] = {0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00},
    ['='] = {0x00, 0x7C, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00},
    ['>'] = {0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00},
    ['?'] = {0x3C, 0x42, 0x02, 0x0C, 0x10, 0x00, 0x10, 0x00},
    ['@'] = {0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x9E, 0x40, 0x3C},

    ['A'] = {0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['B'] = {0x7C, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x7C, 0x00},
    ['C'] = {0x3C, 0x42, 0x40, 0x40, 0x40, 0x42, 0x3C, 0x00},
    ['D'] = {0x78, 0x44, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00},
    ['E'] = {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00},
    ['F'] = {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x40, 0x00},
    ['G'] = {0x3C, 0x42, 0x40, 0x4E, 0x42, 0x42, 0x3C, 0x00},
    ['H'] = {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['I'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    ['J'] = {0x0E, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38, 0x00},
    ['K'] = {0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x00},
    ['L'] = {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00},
    ['M'] = {0x42, 0x66, 0x5A, 0x5A, 0x42, 0x42, 0x42, 0x00},
    ['N'] = {0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x42, 0x00},
    ['O'] = {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['P'] = {0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x00},
    ['Q'] = {0x3C, 0x42, 0x42, 0x42, 0x52, 0x4C, 0x3A, 0x00},
    ['R'] = {0x7C, 0x42, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00},
    ['S'] = {0x3C, 0x40, 0x40, 0x3C, 0x02, 0x02, 0x7C, 0x00},
    ['T'] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U'] = {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['V'] = {0x42, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00},
    ['W'] = {0x42, 0x42, 0x42, 0x5A, 0x5A, 0x66, 0x42, 0x00},
    ['X'] = {0x42, 0x24, 0x18, 0x18, 0x18, 0x24, 0x42, 0x00},
    ['Y'] = {0x42, 0x24, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['Z'] = {0x7E, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7E, 0x00},

    ['['] = {0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C, 0x00},
    ['\\'] = {0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x00, 0x00},
    [']'] = {0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x00},
    ['^'] = {0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['_'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00},
    ['`'] = {0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},

    ['a'] = {0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x3E, 0x00},
    ['b'] = {0x40, 0x40, 0x5C, 0x62, 0x42, 0x62, 0x5C, 0x00},
    ['c'] = {0x00, 0x00, 0x3C, 0x40, 0x40, 0x40, 0x3C, 0x00},
    ['d'] = {0x02, 0x02, 0x3A, 0x46, 0x42, 0x46, 0x3A, 0x00},
    ['e'] = {0x00, 0x00, 0x3C, 0x42, 0x7E, 0x40, 0x3C, 0x00},
    ['f'] = {0x0C, 0x12, 0x10, 0x7C, 0x10, 0x10, 0x10, 0x00},
    ['g'] = {0x00, 0x00, 0x3A, 0x46, 0x46, 0x3A, 0x02, 0x3C},
    ['h'] = {0x40, 0x40, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x00},
    ['i'] = {0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00},
    ['j'] = {0x04, 0x00, 0x0C, 0x04, 0x04, 0x44, 0x44, 0x38},
    ['k'] = {0x40, 0x40, 0x48, 0x50, 0x60, 0x50, 0x48, 0x00},
    ['l'] = {0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00},
    ['m'] = {0x00, 0x00, 0x6C, 0x52, 0x52, 0x42, 0x42, 0x00},
    ['n'] = {0x00, 0x00, 0x5C, 0x62, 0x42, 0x42, 0x42, 0x00},
    ['o'] = {0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['p'] = {0x00, 0x00, 0x5C, 0x62, 0x62, 0x5C, 0x40, 0x40},
    ['q'] = {0x00, 0x00, 0x3A, 0x46, 0x46, 0x3A, 0x02, 0x02},
    ['r'] = {0x00, 0x00, 0x5C, 0x62, 0x40, 0x40, 0x40, 0x00},
    ['s'] = {0x00, 0x00, 0x3C, 0x40, 0x3C, 0x02, 0x7C, 0x00},
    ['t'] = {0x10, 0x10, 0x7C, 0x10, 0x10, 0x12, 0x0C, 0x00},
    ['u'] = {0x00, 0x00, 0x42, 0x42, 0x42, 0x46, 0x3A, 0x00},
    ['v'] = {0x00, 0x00, 0x42, 0x42, 0x24, 0x24, 0x18, 0x00},
    ['w'] = {0x00, 0x00, 0x42, 0x52, 0x52, 0x6C, 0x44, 0x00},
    ['x'] = {0x00, 0x00, 0x42, 0x24, 0x18, 0x24, 0x42, 0x00},
    ['y'] = {0x00, 0x00, 0x42, 0x42, 0x3E, 0x02, 0x3C, 0x00},
    ['z'] = {0x00, 0x00, 0x7E, 0x04, 0x18, 0x20, 0x7E, 0x00},

    ['{'] = {0x0C, 0x10, 0x10, 0x60, 0x10, 0x10, 0x0C, 0x00},
    ['|'] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00},
    ['}'] = {0x30, 0x08, 0x08, 0x06, 0x08, 0x08, 0x30, 0x00},
    ['~'] = {0x00, 0x00, 0x34, 0x4C, 0x00, 0x00, 0x00, 0x00},

    [127 ... 255] = {0},
};

#include <stdint.h>

typedef uint32_t color_t;
typedef uint8_t chan_t;
typedef float alpha_t;

enum Color {
  COLOR_BLACK = 0xFF000000,
  COLOR_WHITE = 0xFFFFFFFF,
  COLOR_RED = 0xFFFF0000,
  COLOR_GREEN = 0xFF00FF00,
  COLOR_BLUE = 0xFF0000FF,
  COLOR_YELLOW = 0xFFFFFF00
};

#define rgb(r, g, b)                                                           \
  ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) |                 \
      ((color_t)255 << 24)
#define rgba(r, g, b, a)                                                       \
  ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) |                 \
      ((uint32_t)(a * 255.0f + 0.5f) << 24)

#define get_alpha(c) ((c >> 24) & 0xFF)
#define get_red(c) ((c >> 16) & 0xFF)
#define get_green(c) ((c >> 8) & 0xFF)
#define get_blue(c) (c & 0xFF)

#define make_color(a, r, g, b)                                                 \
  ((color_t)b << 0) | ((color_t)g << 8) | ((color_t)r << 16) |                 \
      ((color_t)a << 24)

/** @brief Blend two colors using the source alpha */
color_t color_blend(color_t src, color_t dst) {
  uint8_t alpha = get_alpha(src);

  if (alpha == 0)
    return dst;
  if (alpha == 255)
    return src;

  uint8_t inv_alpha = 255 - alpha;

  uint8_t r = (get_red(src) * alpha + get_red(dst) * inv_alpha) / 255;
  uint8_t g = (get_green(src) * alpha + get_green(dst) * inv_alpha) / 255;
  uint8_t b = (get_blue(src) * alpha + get_blue(dst) * inv_alpha) / 255;

  return make_color(255, r, g, b);
}

/** @brief Get glyph bitmap for a character */
static const uint8_t *get_glyph(char c) {
  if ((unsigned char)c >= 128)
    return 0;
  return font[(unsigned char)c];
}

/** @brief Draw a scaled glyph bitmap onto the framebuffer */
static void draw_pixel_array_scaled(const uint8_t *glyph, int pitch, int x,
                                    int y, int w, int h, int scale_x,
                                    int scale_y, color_t font_color) {
  for (int row = 0; row < h; ++row) {
    uint8_t line = glyph[row];

    for (int col = 0; col < w; ++col) {
      if (line & (0x80 >> col))
        for (int dy = 0; dy < scale_y; ++dy)
          for (int dx = 0; dx < scale_x; ++dx) {
            unsigned int px = (unsigned int)(x + col * scale_x + dx);
            unsigned int py = (unsigned int)(y + row * scale_y + dy);

            if (px < g_platform.fb_width && py < g_platform.fb_height) {
              color_t *pixel =
                  &((uint32_t *)g_platform.fb_base)[py * pitch + px];
              color_t dst_color = *pixel;
              color_t blended = color_blend(font_color, dst_color);
              *pixel = blended;
            }
          }
    }
  }
}

/** @brief Draw a single character on the framebuffer */
void draw_char(char c, int x, int y, int w, int h, color_t font_color) {
  const uint8_t *glyph = get_glyph(c);
  if (!glyph)
    glyph = get_glyph('!');

  int pitch = g_platform.fb_pitch / 4;
  draw_pixel_array_scaled(glyph, pitch, x, y, w, h, 1, 1, font_color);
}

extern void jump_to_kernel(void *boot_info, void *entry, uint64_t stack);

/** @brief Allocate a zeroed 4 KiB page table */
static uint64_t *alloc_page_table(uint64_t *next_free) {
  uint64_t *pt = (uint64_t *)(*next_free);
  *next_free += 0x1000;
  for (int i = 0; i < 512; i++)
    pt[i] = 0;
  return pt;
}

/** @brief Map a 2 MiB huge page in the page tables */
static void map_2mb(uint64_t *pml4, uint64_t virt, uint64_t phys,
                    uint64_t *next_free) {
  uint64_t pml4i = (virt >> 39) & 0x1FF;
  uint64_t pdpti = (virt >> 30) & 0x1FF;
  uint64_t pdi = (virt >> 21) & 0x1FF;

  if (!(pml4[pml4i] & 1)) {
    uint64_t *p = alloc_page_table(next_free);
    pml4[pml4i] = (uint64_t)p | 0x3;
  }
  uint64_t *pdpt = (uint64_t *)(pml4[pml4i] & ~0xFFFULL);

  if (!(pdpt[pdpti] & 1)) {
    uint64_t *p = alloc_page_table(next_free);
    pdpt[pdpti] = (uint64_t)p | 0x3;
  }
  uint64_t *pd = (uint64_t *)(pdpt[pdpti] & ~0xFFFULL);

  pd[pdi] = phys | 0x83;
}

/** @brief Map a 4 KiB page in the page tables */
static void map_4kb(uint64_t *pml4, uint64_t virt, uint64_t phys,
                    uint64_t *next_free) {
  uint64_t pml4i = (virt >> 39) & 0x1FF;
  uint64_t pdpti = (virt >> 30) & 0x1FF;
  uint64_t pdi = (virt >> 21) & 0x1FF;
  uint64_t pti = (virt >> 12) & 0x1FF;

  if (!(pml4[pml4i] & 1)) {
    uint64_t *p = alloc_page_table(next_free);
    pml4[pml4i] = (uint64_t)p | 0x3;
  }
  uint64_t *pdpt = (uint64_t *)(pml4[pml4i] & ~0xFFFULL);

  if (!(pdpt[pdpti] & 1)) {
    uint64_t *p = alloc_page_table(next_free);
    pdpt[pdpti] = (uint64_t)p | 0x3;
  }
  uint64_t *pd = (uint64_t *)(pdpt[pdpti] & ~0xFFFULL);

  if (!(pd[pdi] & 1)) {
    uint64_t *p = alloc_page_table(next_free);
    pd[pdi] = (uint64_t)p | 0x3;
  }
  uint64_t *pt = (uint64_t *)(pd[pdi] & ~0xFFFULL);

  pt[pti] = phys | 0x3;
}

static uint64_t align_down(uint64_t value, uint64_t align) {
  return value & ~(align - 1);
}

static uint64_t align_up(uint64_t value, uint64_t align) {
  return (value + align - 1) & ~(align - 1);
}

/** @brief Identity + direct-map a physical range using 2 MiB pages */
static void map_phys_range_2mb(uint64_t *pml4, uint64_t phys_start,
                               uint64_t size, uint64_t *next_free) {
  if (!size)
    return;

  uint64_t start = align_down(phys_start, 0x200000);
  uint64_t end = align_up(phys_start + size, 0x200000);

  for (uint64_t p = start; p < end; p += 0x200000) {
    map_2mb(pml4, p, p, next_free);
    map_2mb(pml4, DIRECT_MAP_BASE + p, p, next_free);
  }
}

static uint64_t min_u64(uint64_t a, uint64_t b) { return a < b ? a : b; }

/** @brief Identity-map all loader context memory ranges */
static void map_loader_context_ranges(uint64_t *pml4, loader_context_t *ctx,
                                      uint64_t *next_free) {
  for (uint64_t i = 0; i < ctx->mem_map_count; i++) {
    mem_descriptor_t *d = &ctx->mem_map[i];
    map_phys_range_2mb(pml4, d->phys_start, d->num_pages * 0x1000, next_free);
  }

  map_phys_range_2mb(pml4, (uint64_t)ctx, sizeof(*ctx), next_free);
  map_phys_range_2mb(pml4, (uint64_t)ctx->mem_map,
                     ctx->mem_map_count * sizeof(ctx->mem_map[0]), next_free);
  map_phys_range_2mb(pml4, (uint64_t)ctx->elf_buf, ctx->elf_size, next_free);

  uint64_t fb_size = (uint64_t)ctx->framebuffer.pitch * ctx->framebuffer.height;
  map_phys_range_2mb(pml4, (uint64_t)ctx->framebuffer.base, fb_size, next_free);
}

/**
 * @brief Load ELF segments into physical memory
 * @return Virtual entry point of the kernel
 */
static uint64_t load_elf(void *elf_buf) {
  Elf64_Ehdr *ehdr = elf_buf;

  if (ehdr->e_ident[0] != 0x7f || ehdr->e_ident[1] != 'E')
    return 0;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)elf_buf + ehdr->e_phoff +
                                    i * ehdr->e_phentsize);

    if (ph->p_type != PT_LOAD)
      continue;

    uint64_t phys = ph->p_vaddr - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;
    uint8_t *src = (uint8_t *)elf_buf + ph->p_offset;
    uint8_t *dst = (uint8_t *)phys;

    for (uint64_t b = 0; b < ph->p_filesz; b++)
      dst[b] = src[b];

    for (uint64_t b = ph->p_filesz; b < ph->p_memsz; b++)
      dst[b] = 0;
  }

  return ehdr->e_entry;
}

/**
 * @brief Calculate the highest physical address needed by kernel ELF
 */
static uint64_t elf_phys_end(void *elf_buf) {
  Elf64_Ehdr *ehdr = elf_buf;
  uint64_t end = KERNEL_PHYS_BASE;

  for (int i = 0; i < ehdr->e_phnum; i++) {
    Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)elf_buf + ehdr->e_phoff +
                                    i * ehdr->e_phentsize);

    if (ph->p_type != PT_LOAD)
      continue;

    uint64_t seg_end =
        (ph->p_vaddr - KERNEL_VIRT_BASE + KERNEL_PHYS_BASE) + ph->p_memsz;
    if (seg_end > end)
      end = seg_end;
  }

  return end;
}

/** @brief Bootloader main entry — sets up page tables, loads ELF, jumps to
 * kernel */
void boot_main(loader_context_t *ctx) {
  uint64_t free_base = ctx->free_phys_base;
  uint64_t free_size = ctx->free_phys_size;
  void *rsdp = ctx->rsdp;
  fb_info_t fb = ctx->framebuffer;
  fb_g = ctx->framebuffer;

  uint64_t kern_phys_end = elf_phys_end(ctx->elf_buf);
  uint64_t arena_start = (ctx->free_phys_base + 0x1FFFFF) & ~0x1FFFFFULL;
  uint64_t pt_arena = arena_start;

  uint64_t *pml4 = alloc_page_table(&pt_arena);

  /* -- 1. Map kernel identity -- */
  for (uint64_t off = 0; off < kern_phys_end - KERNEL_PHYS_BASE; off += 0x1000)
    map_4kb(pml4, KERNEL_VIRT_BASE + off, KERNEL_PHYS_BASE + off, &pt_arena);

  /* -- 2. Recursive mapping -- */
  pml4[510] = (uint64_t)pml4 | 0x3;

  /* -- 3. Map firmware-reported physical ranges -- */
  map_loader_context_ranges(pml4, ctx, &pt_arena);

  uint64_t arena_reserved =
      min_u64(free_size - (arena_start - free_base), 64 * 1024 * 1024);
  map_phys_range_2mb(pml4, arena_start, arena_reserved, &pt_arena);

  /* -- 4. BootInfo allocation after page tables -- */
  uint64_t arena = align_up(pt_arena, 0x1000);
  BootInfo *boot_info = (BootInfo *)arena;
  arena += sizeof(BootInfo);

  /* -- 5. Stack after BootInfo -- */
  uint64_t stack_phys = (arena + 0x1FFFFF) & ~0x1FFFFFULL;
  arena = stack_phys + 16 * 0x1000;
  uint64_t stack_top_virt = DIRECT_MAP_BASE + stack_phys + 16 * 0x1000;

  boot_info->rsdp = rsdp;
  boot_info->framebuffer.base = fb.base;
  boot_info->framebuffer.width = fb.width;
  boot_info->framebuffer.height = fb.height;
  boot_info->framebuffer.pitch = fb.pitch;
  boot_info->framebuffer.bpp = fb.bpp;
  boot_info->memory_map.heap_start = DIRECT_MAP_BASE + arena;
  boot_info->memory_map.heap_size = free_size - (arena - free_base);
  boot_info->memory_map.pml4_phys = (uint64_t)pml4;

  g_platform.fb_base = (uint64_t)boot_info->framebuffer.base;
  g_platform.fb_width = boot_info->framebuffer.width;
  g_platform.fb_height = boot_info->framebuffer.height;
  g_platform.fb_pitch = boot_info->framebuffer.pitch;

  /* -- 6. Load ELF into physical memory -- */
  uint64_t entry_virt = load_elf(ctx->elf_buf);

  /* -- 7. Switch page tables -- */
  set_cr3((uint64_t)pml4);

  draw_char('H', 10, 10, 8, 8, COLOR_WHITE);
  draw_char('u', 18, 10, 8, 8, COLOR_WHITE);
  draw_char('b', 26, 10, 8, 8, COLOR_WHITE);
  draw_char('b', 34, 10, 8, 8, COLOR_WHITE);
  draw_char('l', 42, 10, 8, 8, COLOR_WHITE);
  draw_char('e', 50, 10, 8, 8, COLOR_WHITE);

  /* -- 8. Jump to kernel -- */
  uint64_t boot_info_virt = DIRECT_MAP_BASE + (uint64_t)boot_info;

  jump_to_kernel((void *)boot_info_virt, (void *)entry_virt, stack_top_virt);
}
