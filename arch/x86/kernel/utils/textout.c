#include "utils/font.h"
#include "utils/framebuffer.h"
#include "utils/color.h"

const uint8_t *get_glyph(char c)
{
	if ((unsigned char)c >= 128)
		return 0;	       // Перевірка межі
	return font[(unsigned char)c]; // Повертаємо гліф для символу
}

static inline uint8_t get_alpha(color_t c) { return (c >> 24) & 0xFF; }
static inline uint8_t get_red(color_t c) { return (c >> 16) & 0xFF; }
static inline uint8_t get_green(color_t c) { return (c >> 8) & 0xFF; }
static inline uint8_t get_blue(color_t c) { return c & 0xFF; }

static inline color_t make_color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
	return ((color_t)a << 24) | ((color_t)r << 16) | ((color_t)g << 8) | b;
}

static color_t blend_colors(color_t src, color_t dst)
{
	alpha_t alpha = get_alpha(src);

	if (alpha == 0)
		alpha = 255;

	if (alpha == 255)
		return src;

	alpha_t inv_alpha = 255 - alpha;

	chan_t r = (get_red(src) * alpha + get_red(dst) * inv_alpha) / 255;
	chan_t g = (get_green(src) * alpha + get_green(dst) * inv_alpha) / 255;
	chan_t b = (get_blue(src) * alpha + get_blue(dst) * inv_alpha) / 255;
	alpha_t a = 255;

	return make_color(a, r, g, b);
}

static void draw_pixel_array_scaled(uint8_t *glyph, int pitch, framebuffer_info_t *fb,
				    int x, int y, int w, int h, int scale_x, int scale_y, color_t font_color)
{
	for (int row = 0; row < h; ++row)
	{
		uint8_t line = glyph[row];

		for (int col = 0; col < w; ++col)
		{
			if (line & (0x80 >> col))
				for (int dy = 0; dy < scale_y; ++dy)
					for (int dx = 0; dx < scale_x; ++dx)
					{
						unsigned int px = (unsigned int)(x + col * scale_x + dx);
						unsigned int py = (unsigned int)(y + row * scale_y + dy);

						if (px < fb->width && py < fb->height)
						{
							color_t *pixel = &((uint32_t *)fb->base)[py * pitch + px];
							color_t dst_color = *pixel;
							color_t blended = blend_colors(font_color, dst_color);
							*pixel = blended;
						}
					}
		}
	}
}

void draw_char(framebuffer_info_t *fb, char c, int x, int y, int w, int h, color_t font_color)
{
	uint8_t *glyph = (uint8_t *)get_glyph(c); // Отримуємо гліф символу
	if (glyph == 0)
		glyph = (uint8_t *)get_glyph('!');

	int pitch = fb->pitch / 4; // Вираховуємо ширину рядка в пікселях (з
				   // урахуванням 32 біт на піксель)
	draw_pixel_array_scaled(glyph, pitch, fb, x, y, w, h, 1, 1, font_color);
}

// crutch
void clear_char_area(framebuffer_info_t *fb, int x, int y, int w, int h, color_t bg_color)
{
	int pitch = fb->pitch / 4;

	for (int row = 0; row < h; ++row)
	{
		for (int col = 0; col < w; ++col)
		{
			unsigned int px = x + col;
			unsigned int py = y + row;

			if (px < fb->width && py < fb->height)
			{
				((uint32_t *)fb->base)[py * pitch + px] = bg_color;
			}
		}
	}
}
