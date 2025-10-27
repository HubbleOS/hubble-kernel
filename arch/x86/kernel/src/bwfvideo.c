#include "printk.h"
#include <stdint.h>

#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include <utils/bwfvideo.h>
#include <utils/color.h>
#include <mm/kmalloc.h>

// --- структура кадру ---
struct BWFrameHeader
{
	uint16_t width;
	uint16_t height;
	uint32_t size;
} __attribute__((packed));

void sleep_ms(uint32_t ms)
{
	// Константу підібрати експериментально під свою частоту CPU
	for (volatile uint64_t i = 0; i < (ms * 100000); i++)
	{
		__asm__ __volatile__("nop");
	}
}

// --- твоя функція малювання ---
static inline void putpixel(framebuffer_info_t *bi, int x, int y, color_t color,
			    uint32_t fb_pitch, uint32_t bpp)
{
	uint8_t *ptr = bi->base + y * fb_pitch + x * (bpp / 8);
	*(color_t *)ptr = color;
}

// --- відмалювання одного кадру ---
void draw_frame(framebuffer_info_t *bi, uint8_t *data, int x_start, int y_start)
{
	struct BWFrameHeader *hdr = (struct BWFrameHeader *)data;
	uint8_t *pixels = data + sizeof(struct BWFrameHeader);

	int w = hdr->width;
	int h = hdr->height;
	int row_bytes = (w + 7) / 8;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
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

// --- програвач ---
void play_bwvid(framebuffer_info_t *bi, const char *path, uint32_t pitch, uint32_t bpp, int x, int y)
{
	VFS_File *file = vfs_open(path, VFS_O_RDONLY);
	if (IS_ERR(file))
	{
		printk("Failed to open file %s\n", path);
		return;
	}

	while (1)
	{
		struct BWFrameHeader hdr;

		// читаємо заголовок
		int r = vfs_read(file, &hdr, sizeof(hdr));
		if (r != sizeof(hdr))
			break; // кінець файлу або помилка

		// виділяємо буфер під кадр
		uint8_t *frame_data = kmalloc(sizeof(hdr) + hdr.size);
		if (!frame_data)
			break;

		*(struct BWFrameHeader *)frame_data = hdr;

		// читаємо дані кадру
		r = vfs_read(file, frame_data + sizeof(hdr), hdr.size);
		if (r != (int)hdr.size)
		{
			free(frame_data);
			printk("frame read error %d, expected %d\n", r, hdr.size);
			break;
		}
		printk("frame size: %d\n", hdr.size);
		//  відмальовуємо кадр
		printk("Frame %dx%d, size=%d\n", hdr.width, hdr.height, hdr.size);

		draw_frame(bi, frame_data, x, y);

		free(frame_data);
		sleep_ms(33);
		// тут можна вставити таймер/затримку для FPS
		// наприклад: sleep_ms(33) для ~30 кадрів/с
	}

	// vfs_close(file);
}
