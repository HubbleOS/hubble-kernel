#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fs/vfs/vfs.h>
#include <fs/vfs/vfs_standart_struct.h>
#include "utils/framebuffer.h"

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
static inline void putpixel(int x, int y, uint32_t color,
			    uint32_t fb_pitch, uint32_t bpp)
{
	uint8_t *ptr = boot_info.framebuffer->base + y * fb_pitch + x * (bpp / 8);
	*(uint32_t *)ptr = color;
}

// --- відмалювання одного кадру ---
void draw_frame(uint8_t *data, uint32_t pitch, uint32_t bpp)

{
	struct BWFrameHeader *hdr = (struct BWFrameHeader *)data;
	uint8_t *pixels = data + sizeof(struct BWFrameHeader);

	int w = hdr->width;
	int h = hdr->height;

	uint32_t fb_pitch = boot_info.framebuffer->pitch;
	bpp = boot_info.framebuffer->bpp;

	int row_bytes = (w + 7) / 8; // кількість байтів на рядок у кадрі
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			int byte_index = y * row_bytes + x / 8;
			int bit_index = 7 - (x % 8);
			int bit = (pixels[byte_index] >> bit_index) & 1;
			uint32_t color = bit ? 0xFFFFFFFF : 0x00000000; // білий / чорний

			putpixel(x, y, color, fb_pitch, bpp);
		}
	}
}

// --- програвач ---
void play_bwvid(const char *path, uint32_t pitch, uint32_t bpp)
{
	VFS_File *file = vfs_open(path, VFS_O_RDONLY);
	if (IS_ERR(file))
	{
		// printf("Не вдалося відкрити файл\n");
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
		uint8_t *frame_data = malloc(sizeof(hdr) + hdr.size);
		if (!frame_data)
			break;

		*(struct BWFrameHeader *)frame_data = hdr;

		// читаємо дані кадру
		r = vfs_read(file, frame_data + sizeof(hdr), hdr.size);
		if (r != (int)hdr.size)
		{
			free(frame_data);
			printf("frame read error %d, expected %d\n", r, hdr.size);
			break;
		}
		// printf("frame size: %d\n", hdr.size);
		//  відмальовуємо кадр
		printf("Frame %dx%d, size=%d\n", hdr.width, hdr.height, hdr.size);

		draw_frame(frame_data, pitch, bpp);

		free(frame_data);
		sleep_ms(33);
		// тут можна вставити таймер/затримку для FPS
		// наприклад: sleep_ms(33) для ~30 кадрів/с
	}

	// vfs_close(file);
}
