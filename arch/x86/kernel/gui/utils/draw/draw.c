#include "draw.h"

static int isqrt(int n)
{
	if (n <= 0)
		return 0;
	int x = n, y = 1;
	while (x > y)
	{
		x = (x + y) / 2;
		y = n / x;
	}
	return x;
}

static color_t blend_corner(color_t bg, int dx, int dy, int r, int feather)
{
	int dist256 = isqrt((dx * dx + dy * dy) * 65536);
	int r256 = r * 256;
	int f256 = feather * 256;
	int delta256 = dist256 - r256;

	if (delta256 >= f256)
		return rgba(0, 0, 0, 0);
	if (delta256 <= 0)
		return bg;

	uint8_t a = (uint8_t)(((f256 - delta256) * (int)(bg >> 24)) / f256);
	return (bg & 0x00FFFFFF) | ((uint32_t)a << 24);
}

void draw_rounded_rect(uint32_t *buffer, int buf_w, int buf_h,
		       int x, int y, int w, int h,
		       int radius, int feather, color_t color)
{
	for (int row = 0; row < h; row++)
	{
		for (int col = 0; col < w; col++)
		{
			int px = x + col;
			int py = y + row;
			if (px < 0 || py < 0 || px >= buf_w || py >= buf_h)
				continue;

			color_t out = color;

			if (radius > 0)
			{
				int cx = -1, cy = -1;
				if (col < radius && row < radius)
				{
					cx = radius;
					cy = radius;
				}
				else if (col >= w - radius && row < radius)
				{
					cx = w - radius;
					cy = radius;
				}
				else if (col < radius && row >= h - radius)
				{
					cx = radius;
					cy = h - radius;
				}
				else if (col >= w - radius && row >= h - radius)
				{
					cx = w - radius;
					cy = h - radius;
				}
				if (cx != -1)
					out = blend_corner(color, col - cx, row - cy, radius, feather);
			}

			buffer[py * buf_w + px] = out;
		}
	}
}
