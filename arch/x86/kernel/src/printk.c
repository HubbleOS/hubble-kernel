#include "printk.h"
#include "utils/font.h"
#include <utils/color.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "io.h"
#include <hpet/hpet.h>
#include <smp/spinlock.h>

#include <asm.h>

// Ring buffer for logs
static char log_buffer[PRINTK_BUFFER_SIZE];
static size_t log_head = 0;	 // Position records
static size_t log_tail = 0;	 // Reading position
static size_t log_size = 0;	 // Current data size
static bool log_wrapped = false; // Buffer overflowed

// Early console
static int early_x = 0;
static int early_y = 0;
static framebuffer_info_t *early_fb = NULL;
static bool early_mode = false;

// Console callback
static void (*console_write)(const char *buf, size_t len, void *data) = NULL;
static void *console_user_data = NULL;

// lock
static spinlock_t printk_lock = SPINLOCK_INIT("printk");

// Add to ring buffer
static void log_buffer_append(const char *buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		log_buffer[log_head] = buf[i];
		log_head = (log_head + 1) % PRINTK_BUFFER_SIZE;

		if (log_size < PRINTK_BUFFER_SIZE)
		{
			log_size++;
		}
		else
		{
			// Буфер полон, перемещаем tail
			log_wrapped = true;
			log_tail = (log_tail + 1) % PRINTK_BUFFER_SIZE;
		}
	}
}

// Early console functions (как ваш старый код)
static void early_putchar(char c)
{
	// out to serial
	outb(0x3f8, c);

	if (!early_fb || !early_fb->base)
		return;

	if (c == '\n')
	{
		early_x = 0;
		early_y += CHAR_HEIGHT;

		if (early_y + CHAR_HEIGHT > early_fb->height)
		{
			// Scroll
			size_t line_size = early_fb->pitch;
			uint8_t *fb_base = (uint8_t *)early_fb->base;

			for (int y = 0; y < early_fb->height - CHAR_HEIGHT; y++)
			{
				memcpy(fb_base + y * line_size,
				       fb_base + (y + CHAR_HEIGHT) * line_size,
				       line_size);
			}

			for (int y = early_fb->height - CHAR_HEIGHT; y < early_fb->height; y++)
			{
				memset(fb_base + y * line_size, 0, line_size);
			}

			early_y = early_fb->height - CHAR_HEIGHT;
		}
		return;
	}

	if (c == '\r')
	{
		early_x = 0;
		return;
	}

	if (c == '\b')
	{
		if (early_x >= CHAR_WIDTH)
		{
			early_x -= CHAR_WIDTH;
			// clear_char_area(early_fb, early_x, early_y, CHAR_WIDTH, CHAR_HEIGHT, 0x000000);
		}
		return;
	}

	if (c == '\t')
	{
		for (int i = 0; i < 4; i++)
			early_putchar(' ');
		return;
	}

	if (early_x + CHAR_WIDTH > early_fb->width)
		early_putchar('\n');

	draw_char(early_fb, c, early_x, early_y, CHAR_WIDTH, CHAR_HEIGHT, COLOR_WHITE);
	early_x += CHAR_WIDTH;
}

static void early_puts(const char *s, size_t len)
{
	for (size_t i = 0; i < len; i++)
		early_putchar(s[i]);
}

// Checking the severity level
static bool is_critical_level(const char *fmt)
{
	if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>')
	{
		int level = fmt[1] - '0';
		return level <= 2; // EMERG, ALERT, CRIT
	}
	return false;
}

void early_printk_init(framebuffer_info_t *fb)
{
	early_fb = fb;
	early_x = 0;
	early_y = 0;
	early_mode = true;
}

void printk_init(framebuffer_info_t *fb)
{
	early_fb = fb;
	early_x = 0;
	early_y = 0;
}

void printk_register_console(void (*write_fn)(const char *buf, size_t len, void *data), void *user_data)
{
	console_write = write_fn;
	console_user_data = user_data;
	early_mode = false;

	// We output accumulated logs to the terminal
	if (console_write && log_size > 0)
	{
		size_t pos = log_tail;
		size_t remaining = log_size;

		while (remaining > 0)
		{
			size_t chunk = remaining;
			if (pos + chunk > PRINTK_BUFFER_SIZE)
				chunk = PRINTK_BUFFER_SIZE - pos;

			console_write(log_buffer + pos, chunk, console_user_data);
			pos = (pos + chunk) % PRINTK_BUFFER_SIZE;
			remaining -= chunk;
		}
	}
}

void printk_unregister_console(void)
{
	console_write = NULL;
	console_user_data = NULL;
}

void printk_set_early_mode(bool enable)
{
	early_mode = enable;
}

bool printk_is_early_mode(void)
{
	return early_mode;
}

size_t printk_get_log_buffer(char *dest, size_t max_len)
{
	if (!dest || max_len == 0)
		return 0;

	size_t to_copy = log_size < max_len ? log_size : max_len;
	size_t copied = 0;
	size_t pos = log_tail;

	while (copied < to_copy)
	{
		dest[copied++] = log_buffer[pos];
		pos = (pos + 1) % PRINTK_BUFFER_SIZE;
	}

	return copied;
}

void printk_clear_log_buffer(void)
{
	log_head = 0;
	log_tail = 0;
	log_size = 0;
	log_wrapped = false;
}

size_t printk_get_log_size(void)
{
	return log_size;
}

static void printk_pad(char c, int count)
{
	char buf[256];
	int written = 0;

	while (count > 0)
	{
		int chunk = count > sizeof(buf) ? sizeof(buf) : count;
		memset(buf, c, chunk);

		// Write to the buffer
		log_buffer_append(buf, chunk);

		// Display it on the screen if necessary
		if (early_mode && early_fb)
			early_puts(buf, chunk);
		else if (console_write)
			console_write(buf, chunk, console_user_data);

		count -= chunk;
	}
}

#define FLAG_LEFT_ADJUST (1U << 0)
#define FLAG_SHOW_SIGN (1U << 1)
#define FLAG_SPACE (1U << 2)
#define FLAG_ALT_FORM (1U << 3)
#define FLAG_ZERO_PAD (1U << 4)

enum length_mod
{
	LEN_NONE,
	LEN_HH,
	LEN_H,
	LEN_L,
	LEN_LL,
	LEN_Z,
	LEN_T,
	LEN_J,
};

static const char *parse_flags(const char *fmt, unsigned *flags)
{
	*flags = 0;
	while (*fmt)
	{
		switch (*fmt)
		{
		case '-':
			*flags |= FLAG_LEFT_ADJUST;
			fmt++;
			break;
		case '+':
			*flags |= FLAG_SHOW_SIGN;
			fmt++;
			break;
		case ' ':
			*flags |= FLAG_SPACE;
			fmt++;
			break;
		case '#':
			*flags |= FLAG_ALT_FORM;
			fmt++;
			break;
		case '0':
			*flags |= FLAG_ZERO_PAD;
			fmt++;
			break;
		default:
			return fmt;
		}
	}
	return fmt;
}

static const char *parse_width(const char *fmt, int *width, va_list *args)
{
	if (*fmt == '*')
	{
		*width = va_arg(*args, int);
		if (*width < 0)
			*width = -*width;
		return fmt + 1;
	}

	*width = 0;
	while (*fmt >= '0' && *fmt <= '9')
	{
		*width = *width * 10 + (*fmt - '0');
		fmt++;
	}
	return fmt;
}

static const char *parse_precision(const char *fmt, int *precision, va_list *args)
{
	if (*fmt != '.')
	{
		*precision = -1;
		return fmt;
	}

	fmt++;
	if (*fmt == '*')
	{
		*precision = va_arg(*args, int);
		if (*precision < 0)
			*precision = -1;
		return fmt + 1;
	}

	*precision = 0;
	while (*fmt >= '0' && *fmt <= '9')
	{
		*precision = *precision * 10 + (*fmt - '0');
		fmt++;
	}
	return fmt;
}

static const char *parse_length(const char *fmt, enum length_mod *length)
{
	*length = LEN_NONE;

	if (*fmt == 'h')
	{
		fmt++;
		if (*fmt == 'h')
		{
			*length = LEN_HH;
			fmt++;
		}
		else
			*length = LEN_H;
	}
	else if (*fmt == 'l')
	{
		fmt++;
		if (*fmt == 'l')
		{
			*length = LEN_LL;
			fmt++;
		}
		else
			*length = LEN_L;
	}
	else if (*fmt == 'z')
	{
		*length = LEN_Z;
		fmt++;
	}
	else if (*fmt == 't')
	{
		*length = LEN_T;
		fmt++;
	}
	else if (*fmt == 'j')
	{
		*length = LEN_J;
		fmt++;
	}

	return fmt;
}

static char *format_uint(uintmax_t num, char *buf, int base, bool uppercase, int precision)
{
	static const char digits_lower[] = "0123456789abcdef";
	static const char digits_upper[] = "0123456789ABCDEF";
	const char *digits = uppercase ? digits_upper : digits_lower;

	char *ptr = buf + 64;
	*ptr = '\0';

	if (num == 0)
	{
		*--ptr = '0';
		return ptr;
	}

	while (num > 0)
	{
		*--ptr = digits[num % base];
		num /= base;
	}

	int len = (buf + 64) - ptr;
	while (len < precision)
	{
		*--ptr = '0';
		len++;
	}

	return ptr;
}

static char *format_int(intmax_t num, char *buf, bool *is_negative)
{
	*is_negative = false;
	if (num < 0)
	{
		*is_negative = true;
		num = -num;
	}
	return format_uint((uintmax_t)num, buf, 10, false, 0);
}

static void output_string(const char *str, size_t len)
{
	// We always write to the log buffer
	log_buffer_append(str, len);

	// Display on screen
	if (early_mode && early_fb)
	{
		early_puts(str, len);
	}
	else if (console_write)
	{
		console_write(str, len, console_user_data);
	}
}

static void output_formatted(const char *str, int str_len, int width,
			     unsigned flags, char pad_char,
			     const char *prefix, int prefix_len)
{
	int total_len = str_len + prefix_len;
	int padding = (width > total_len) ? (width - total_len) : 0;

	if (flags & FLAG_LEFT_ADJUST)
	{
		if (prefix_len > 0)
			output_string(prefix, prefix_len);
		output_string(str, str_len);
		printk_pad(' ', padding);
	}
	else
	{
		if ((flags & FLAG_ZERO_PAD) && !(flags & FLAG_LEFT_ADJUST))
		{
			if (prefix_len > 0)
				output_string(prefix, prefix_len);
			printk_pad('0', padding);
			output_string(str, str_len);
		}
		else
		{
			printk_pad(pad_char, padding);
			if (prefix_len > 0)
				output_string(prefix, prefix_len);
			output_string(str, str_len);
		}
	}
}

void vprintk(const char *fmt, va_list args)
{
	bool is_critical = is_critical_level(fmt);

	// Skiping the level prefix
	if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '7' && fmt[2] == '>')
		fmt += 3;

	char buf[128];
	va_list args_copy;
	va_copy(args_copy, args);

	while (*fmt)
	{
		if (*fmt != '%')
		{
			char c = *fmt++;
			output_string(&c, 1);
			continue;
		}

		fmt++;

		if (*fmt == '%')
		{
			char c = '%';
			output_string(&c, 1);
			fmt++;
			continue;
		}

		unsigned flags;
		fmt = parse_flags(fmt, &flags);

		int width;
		fmt = parse_width(fmt, &width, &args_copy);

		int precision;
		fmt = parse_precision(fmt, &precision, &args_copy);

		enum length_mod length;
		fmt = parse_length(fmt, &length);

		char specifier = *fmt++;

		char prefix[8] = {0};
		int prefix_len = 0;
		char *str;
		int str_len;
		bool is_negative;
		uintmax_t uval;
		intmax_t ival;

		switch (specifier)
		{
		case 'd':
		case 'i':
			switch (length)
			{
			case LEN_HH:
				ival = (signed char)va_arg(args_copy, int);
				break;
			case LEN_H:
				ival = (short)va_arg(args_copy, int);
				break;
			case LEN_L:
				ival = va_arg(args_copy, long);
				break;
			case LEN_LL:
				ival = va_arg(args_copy, long long);
				break;
			case LEN_Z:
				ival = va_arg(args_copy, size_t);
				break;
			case LEN_T:
				ival = va_arg(args_copy, ptrdiff_t);
				break;
			case LEN_J:
				ival = va_arg(args_copy, intmax_t);
				break;
			default:
				ival = va_arg(args_copy, int);
				break;
			}

			str = format_int(ival, buf, &is_negative);
			str_len = strlen(str);

			if (is_negative)
				prefix[prefix_len++] = '-';
			else if (flags & FLAG_SHOW_SIGN)
				prefix[prefix_len++] = '+';
			else if (flags & FLAG_SPACE)
				prefix[prefix_len++] = ' ';

			if (precision >= 0)
				flags &= ~FLAG_ZERO_PAD;

			output_formatted(str, str_len, width, flags, ' ', prefix, prefix_len);
			break;

		case 'u':
		case 'o':
		case 'x':
		case 'X':
			switch (length)
			{
			case LEN_HH:
				uval = (unsigned char)va_arg(args_copy, unsigned int);
				break;
			case LEN_H:
				uval = (unsigned short)va_arg(args_copy, unsigned int);
				break;
			case LEN_L:
				uval = va_arg(args_copy, unsigned long);
				break;
			case LEN_LL:
				uval = va_arg(args_copy, unsigned long long);
				break;
			case LEN_Z:
				uval = va_arg(args_copy, size_t);
				break;
			case LEN_T:
				uval = va_arg(args_copy, ptrdiff_t);
				break;
			case LEN_J:
				uval = va_arg(args_copy, uintmax_t);
				break;
			default:
				uval = va_arg(args_copy, unsigned int);
				break;
			}

			int base = (specifier == 'o') ? 8 : ((specifier == 'u') ? 10 : 16);
			str = format_uint(uval, buf, base, specifier == 'X', precision);
			str_len = strlen(str);

			if ((flags & FLAG_ALT_FORM) && uval != 0)
			{
				if (specifier == 'o')
				{
					if (str[0] != '0')
						prefix[prefix_len++] = '0';
				}
				else if (specifier == 'x' || specifier == 'X')
				{
					prefix[prefix_len++] = '0';
					prefix[prefix_len++] = specifier;
				}
			}

			if (precision >= 0)
				flags &= ~FLAG_ZERO_PAD;

			output_formatted(str, str_len, width, flags, ' ', prefix, prefix_len);
			break;

		case 'p':
			uval = (uintptr_t)va_arg(args_copy, void *);
			str = format_uint(uval, buf, 16, false, sizeof(void *) * 2);
			str_len = strlen(str);

			prefix[prefix_len++] = '0';
			prefix[prefix_len++] = 'x';

			output_formatted(str, str_len, width, flags, ' ', prefix, prefix_len);
			break;

		case 's':
		{
			const char *s = va_arg(args_copy, const char *);
			if (!s)
				s = "(null)";

			str_len = strlen(s);
			if (precision >= 0 && str_len > precision)
				str_len = precision;

			output_formatted(s, str_len, width, flags, ' ', NULL, 0);
			break;
		}

		case 'c':
		{
			char c = (char)va_arg(args_copy, int);
			buf[0] = c;
			output_formatted(buf, 1, width, flags, ' ', NULL, 0);
			break;
		}

		case 'n':
			break;

		default:
		{
			char tmp[2] = {'%', specifier};
			output_string(tmp, 2);
			break;
		}
		}
	}

	va_end(args_copy);

	// For critical messages add some delay
	if (is_critical && early_mode && early_fb)
	{
		for (volatile int i = 0; i < 10000000; i++)
			;
	}
}

void printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	uint64_t flags;
	asm volatile("pushfq; pop %0; cli" : "=r"(flags));

	spinlock_acquire(&printk_lock);

	vprintk(fmt, args);
	spinlock_release(&printk_lock);

	// Restore interrupt state
	if (flags & (1ULL << 9))
		sti();

	va_end(args);
}
