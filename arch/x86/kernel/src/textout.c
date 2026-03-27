#include <bootinfo/framebuffer.h>
#include <utils/font.h>
#include <utils/color.h>
// #include <utils/color/color.h>

static uint8_t *get_glyph(char c)
{
	if ((unsigned char)c >= 128)
		return 0;				   // Перевірка межі
	return font[(unsigned char)c]; // Повертаємо гліф для символу
}

color_t color_blend(color_t src, color_t dst)
{
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
							color_t blended = color_blend(font_color, dst_color);
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
