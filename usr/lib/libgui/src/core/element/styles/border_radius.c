#include "border_radius.h"

static void get_corner_center(int x, int y, int w, int h, int r, int *cx, int *cy)
{
	*cx = -1;
	*cy = -1;

	if (x < r && y < r)
	{
		*cx = r;
		*cy = r;
	}
	else if (x >= w - r && y < r)
	{
		*cx = w - r;
		*cy = r;
	}
	else if (x < r && y >= h - r)
	{
		*cx = r;
		*cy = h - r;
	}
	else if (x >= w - r && y >= h - r)
	{
		*cx = w - r;
		*cy = h - r;
	}
}

bool border_radius_is_transparent(int x, int y, int w, int h, int r)
{
	if (r <= 0)
		return false;

	int cx, cy;
	get_corner_center(x, y, w, h, r, &cx, &cy);
	if (cx == -1)
		return false;

	int dx = x - cx;
	int dy = y - cy;
	return dx * dx + dy * dy > r * r;
}
