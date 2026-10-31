#include "font.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fonts/font.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

static const char *const face_files[FONT_FACES] = {
    [FONT_SANS] = FONT_DIR "/DejaVuSans.ttf",
    [FONT_MONO] = FONT_DIR "/DejaVuSansMono.ttf",
};

/* A face's file, read once. */
static struct {
  bool tried;
  bool ok;
  unsigned char *data;
  stbtt_fontinfo info;
} faces[FONT_FACES];

typedef struct {
  bool done;
  uint8_t *coverage; /* w * h, 0..255 */
  int w, h;
  int xoff, yoff; /* from the pen position on the baseline */
  int advance;
} glyph_t;

#define CACHED 256 /* Latin-1 is kept; the rest is drawn and dropped */

struct font {
  font_face_t face;
  int px;
  stbtt_fontinfo *info; /* NULL: the 8x8 bitmap font, times `bitmap` */
  int bitmap;
  float scale;
  int ascent, descent, line;
  glyph_t glyphs[CACHED];
  struct font *next;
};

static font_t *fonts;

static bool load_face(font_face_t face) {
  if (faces[face].tried)
    return faces[face].ok;
  faces[face].tried = true;

  int fd = open(face_files[face], O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return false;
  struct stat st;
  unsigned char *data = NULL;
  size_t got = 0;
  if (fstat(fd, &st) == 0 && st.st_size > 0 &&
      (data = malloc(st.st_size))) {
    while (got < (size_t)st.st_size) {
      ssize_t n = read(fd, data + got, st.st_size - got);
      if (n <= 0)
        break;
      got += n;
    }
  }
  close(fd);
  if (!data || got != (size_t)st.st_size ||
      !stbtt_InitFont(&faces[face].info, data,
                      stbtt_GetFontOffsetForIndex(data, 0))) {
    free(data);
    return false;
  }
  faces[face].data = data;
  faces[face].ok = true;
  return true;
}

static int round_up(float v) {
  int i = (int)v;
  return v > i ? i + 1 : i;
}

font_t *font_get(font_face_t face, int px) {
  if ((unsigned)face >= FONT_FACES)
    face = FONT_SANS;
  px = px < 6 ? 6 : px > 96 ? 96 : px;
  for (font_t *f = fonts; f; f = f->next)
    if (f->face == face && f->px == px)
      return f;

  static font_t fallback; /* when out of memory: the 8x8 font */
  font_t *f = calloc(1, sizeof(*f));
  if (!f) {
    fallback = (font_t){.face = face, .px = 8, .bitmap = 1, .ascent = 8,
                        .descent = 2, .line = 11};
    return &fallback;
  }
  f->face = face;
  f->px = px;
  if (load_face(face)) {
    f->info = &faces[face].info;
    f->scale = stbtt_ScaleForPixelHeight(f->info, px);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(f->info, &ascent, &descent, &gap);
    f->ascent = round_up(ascent * f->scale);
    f->descent = round_up(-descent * f->scale);
    f->line = f->ascent + f->descent + (int)(gap * f->scale + 0.5f);
  } else {
    f->bitmap = px >= 16 ? px / 8 : 1;
    f->ascent = 8 * f->bitmap;
    f->descent = 2 * f->bitmap;
    f->line = 11 * f->bitmap;
  }
  f->next = fonts;
  fonts = f;
  return f;
}

int font_ascent(const font_t *f) { return f->ascent; }
int font_descent(const font_t *f) { return f->descent; }
int font_line_height(const font_t *f) { return f->line; }

/* Render @p cp into @p g; '?' for what the face doesn't have. */
static void render(font_t *f, uint32_t cp, glyph_t *g) {
  *g = (glyph_t){.done = true};
  if (!f->info) {
    const uint8_t *bits = cp < 128 ? get_glyph((char)cp) : NULL;
    if (!bits)
      bits = get_glyph('?');
    int s = f->bitmap;
    g->w = g->h = 8 * s;
    g->yoff = -8 * s;
    g->advance = 8 * s;
    g->coverage = calloc(g->w * g->h, 1);
    if (g->coverage)
      for (int y = 0; y < g->h; y++)
        for (int x = 0; x < g->w; x++)
          if (bits[y / s] & (0x80 >> (x / s)))
            g->coverage[y * g->w + x] = 255;
    return;
  }
  if (!stbtt_FindGlyphIndex(f->info, cp))
    cp = '?';
  int adv, lsb;
  stbtt_GetCodepointHMetrics(f->info, cp, &adv, &lsb);
  g->advance = (int)(adv * f->scale + 0.5f);
  g->coverage = stbtt_GetCodepointBitmap(f->info, 0, f->scale, cp, &g->w,
                                         &g->h, &g->xoff, &g->yoff);
}

/* @p cp's glyph: the cached one, or @p tmp, which the caller frees. */
static glyph_t *glyph(font_t *f, uint32_t cp, glyph_t *tmp) {
  glyph_t *g = cp < CACHED ? &f->glyphs[cp] : tmp;
  if (g == tmp)
    tmp->done = false;
  if (!g->done)
    render(f, cp, g);
  return g;
}

static void drop(glyph_t *g, glyph_t *tmp) {
  if (g == tmp)
    free(tmp->coverage); /* stbtt's bitmaps come from malloc() too */
}

/* The next character of UTF-8 @p *s, advancing it; '?' for bad bytes. */
static uint32_t next_char(const char **s) {
  const unsigned char *p = (const unsigned char *)*s;
  uint32_t c = *p++;
  int more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  if (c >= 0x80 && !more) {
    *s = (const char *)p;
    return '?';
  }
  if (more)
    c &= 0x3F >> more;
  for (; more; more--) {
    if ((*p & 0xC0) != 0x80) {
      c = '?';
      break;
    }
    c = c << 6 | (*p++ & 0x3F);
  }
  *s = (const char *)p;
  return c;
}

int font_advance(font_t *f, uint32_t cp) {
  glyph_t tmp, *g = glyph(f, cp, &tmp);
  int adv = g->advance;
  drop(g, &tmp);
  return adv;
}

int font_text_width(font_t *f, const char *text) {
  int w = 0;
  while (text && *text)
    w += font_advance(f, next_char(&text));
  return w;
}

int font_draw_char(font_t *f, uint32_t *buf, int buf_w, int buf_h, int x,
                   int baseline, uint32_t cp, color_t color) {
  glyph_t tmp, *g = glyph(f, cp, &tmp);
  uint32_t alpha = get_alpha(color), rgb = color & 0x00FFFFFF;
  for (int gy = 0; g->coverage && gy < g->h; gy++) {
    int py = baseline + g->yoff + gy;
    if (py < 0 || py >= buf_h)
      continue;
    for (int gx = 0; gx < g->w; gx++) {
      int px = x + g->xoff + gx;
      uint32_t a = g->coverage[gy * g->w + gx];
      if (px < 0 || px >= buf_w || !a)
        continue;
      color_t src = rgb | (a * alpha / 255) << 24;
      uint32_t *dst = &buf[py * buf_w + px];
      *dst = get_alpha(*dst) ? color_blend(src, *dst) : src;
    }
  }
  int adv = g->advance;
  drop(g, &tmp);
  return x + adv;
}

int font_draw(font_t *f, uint32_t *buf, int buf_w, int buf_h, int x,
              int baseline, const char *text, color_t color) {
  while (text && *text && x < buf_w)
    x = font_draw_char(f, buf, buf_w, buf_h, x, baseline, next_char(&text),
                       color);
  return x;
}
