#include "utils/font.h"
#include "utils/framebuffer.h"
#include <stdarg.h>

int cursor_x = 0;
int cursor_y = 0;
framebuffer_info_t *g_fb = 0;
color font_color = COLOR_WHITE;

void init_font(framebuffer_info_t *fb)
{
    g_fb = fb;
    cursor_x = 0;
    cursor_y = 0;
    font_color = rgb(255, 255, 255);
}

const uint8_t *get_glyph(char c)
{
    if ((unsigned char)c >= 128)
        return 0;                  // Перевірка межі
    return font[(unsigned char)c]; // Повертаємо гліф для символу
}

static inline uint8_t get_alpha(color c) { return (c >> 24) & 0xFF; }
static inline uint8_t get_red(color c) { return (c >> 16) & 0xFF; }
static inline uint8_t get_green(color c) { return (c >> 8) & 0xFF; }
static inline uint8_t get_blue(color c) { return c & 0xFF; }

static inline color make_color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    return ((color)a << 24) | ((color)r << 16) | ((color)g << 8) | b;
}

static color blend_colors(color src, color dst)
{
    uint8_t alpha = get_alpha(src);

    // Если альфа == 0, считаем, что в цвете нет альфа-канала (т.е. формат RGB)
    // В этом случае считаем цвет полностью непрозрачным.
    if (alpha == 0)
        alpha = 255;

    if (alpha == 255)
        return src;

    uint8_t inv_alpha = 255 - alpha;

    uint8_t r = (get_red(src) * alpha + get_red(dst) * inv_alpha) / 255;
    uint8_t g = (get_green(src) * alpha + get_green(dst) * inv_alpha) / 255;
    uint8_t b = (get_blue(src) * alpha + get_blue(dst) * inv_alpha) / 255;
    uint8_t a = 255;

    return make_color(a, r, g, b);
}

void draw_pixel_array(uint8_t *glyph, int pitch, framebuffer_info_t *fb, int x, int y)
{
    for (int row = 0; row < CHAR_HEIGHT; ++row)
    {
        uint8_t line = glyph[row];

        for (int col = 0; col < CHAR_WIDTH; ++col)
        {
            if (line & (0x80 >> col))
            {
                unsigned int px = (unsigned int)(x + col);
                unsigned int py = (unsigned int)(y + row);

                if (px < fb->width && py < fb->height)
                {
                    uint32_t *pixel = &((uint32_t *)fb->base)[py * pitch + px];
                    color dst_color = *pixel;
                    color blended = blend_colors(font_color, dst_color);
                    *pixel = blended;
                }
            }
        }
    }
}

void draw_char(framebuffer_info_t *fb, char c, int x, int y)
{
    uint8_t *glyph = (uint8_t *)get_glyph(c); // Отримуємо гліф символу
    if (glyph == 0)
        get_glyph('!');
    int pitch = fb->pitch / 4; // Вираховуємо ширину рядка в пікселях (з урахуванням 32 біт на піксель)

    draw_pixel_array(glyph, pitch, fb, x, y);
}
