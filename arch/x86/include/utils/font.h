#pragma once
#include <stdint.h>
#include "framebuffer.h"
#include "utils/color.h"

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8

// Шрифт для символів 'A' до 'Z'
extern const uint8_t font[131][8];

extern framebuffer_info_t *g_fb;
extern int cursor_x;
extern int cursor_y;
extern color font_color;

void init_font(framebuffer_info_t *fb);

// Вивід одного символу (тільки великі латинські літери)
void draw_char(framebuffer_info_t *fb, char c, int x, int y);

// Порт данных клавиатуры
#define KBD_DATA_PORT 0x60
// Порт статуса клавиатуры
#define KBD_STATUS_PORT 0x64

static inline uint8_t inb(uint16_t port)
{
	uint8_t val;
	__asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
	return val;
}

static inline uint8_t kbd_read_scancode(void)
{
	// Ждем, пока в буфере данных клавиатуры появится символ
	while (!(inb(KBD_STATUS_PORT) & 0x01))
		;

	return inb(KBD_DATA_PORT);
}
