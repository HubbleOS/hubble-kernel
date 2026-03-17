#include "rect.h"

bool rects_intersect(rect_t a, rect_t b)
{
	return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
		 a.y + a.h <= b.y || b.y + b.h <= a.y);
}

rect_t rect_union(rect_t a, rect_t b)
{
	int x1 = a.x < b.x ? a.x : b.x;
	int y1 = a.y < b.y ? a.y : b.y;
	int x2 = (a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
	int y2 = (a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
	rect_t r = {x1, y1, x2 - x1, y2 - y1};
	return r;
}

void merge_dirty_rects(rect_t *dirty, int *count)
{
	bool merged_any;
	do
	{
		merged_any = false;
		for (int i = 0; i < *count; i++)
		{
			for (int j = i + 1; j < *count; j++)
			{
				if (rects_intersect(dirty[i], dirty[j]))
				{
					dirty[i] = rect_union(dirty[i], dirty[j]);
					dirty[j] = dirty[*count - 1];
					(*count)--;
					merged_any = true;
					j--;
				}
			}
		}
	} while (merged_any);
}
