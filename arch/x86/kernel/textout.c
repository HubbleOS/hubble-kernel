#include "utils/font.h"
#include "utils/framebuffer.h"
#include <stdarg.h>

uint8_t *get_glyph(char c)
{
    if (c < 0 || c >= 131)
        return 0;            // Перевірка межі
    return font[(uint8_t)c]; // Повертаємо гліф для символу
}

void draw_pixel_array(uint8_t *glyph, int pitch, framebuffer_info_t *fb, int x, int y, uint32_t color, uint32_t bg_color)
{

    for (int row = 0; row < CHAR_HEIGHT; ++row)
    {
        uint8_t line = glyph[row]; // Отримуємо поточний рядок гліфа

        for (int col = 0; col < CHAR_WIDTH; ++col)
        {
            if (line & (0x80 >> col))
            {                     // Перевіряємо, чи піксель повинен бути включений
                int px = x + col; // Обчислюємо x-координату пікселя
                int py = y + row; // Обчислюємо y-координату пікселя
                if (px >= 0 && px < fb->width && py >= 0 && py < fb->height)
                    ((uint32_t *)fb->base)[py * pitch + px] = color; // Малюємо піксель
            }
            else
            {
                int px = x + col; // Обчислюємо x-координату пікселя
                int py = y + row; // Обчислюємо y-координату пікселя
                if (px >= 0 && px < fb->width && py >= 0 && py < fb->height)
                    ((uint32_t *)fb->base)[py * pitch + px] = 0x000000;
            }
        }
    }
}

void draw_char(framebuffer_info_t *fb, char c, int x, int y, uint32_t color, ...)
{
    uint8_t *glyph = get_glyph(c); // Отримуємо гліф символу
    if (glyph == 0)
        get_glyph('!');
    int pitch = fb->pitch / 4; // Вираховуємо ширину рядка в пікселях (з урахуванням 32 біт на піксель)

    va_list args;
    uint8_t bg_color = 0x000000;

    if (args)
    {
        va_start(args, bg_color);
        bg_color = va_arg(args, uint32_t);
        va_end(args);
    }

    draw_pixel_array(glyph, pitch, fb, x, y, color, bg_color);
}

void print_text(framebuffer_info_t *fb, const char *str, int x, int y, uint32_t color)
{
    while (*str)
    {
        if (1)
        {
            draw_char(fb, *str, x, y, color);
            x += CHAR_WIDTH;
        }
        else if (*str == ' ')
        {
            x += CHAR_WIDTH;
        }
        ++str;
    }
}
