/*
 * printk — kernel logging and formatted output.
 *
 * Provides buffered, colorized log output with ring-buffer storage,
 * pluggable output backends (early serial, full console), and
 * level-based prefix formatting.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include <hubble/color.h>
#include <hubble/printk.h>
#include <hubble/string.h>

#include <asm.h>
#include <smp/spinlock.h>

/* ── Ring buffer ──────────────────────────────────────────────────────────── */

static char log_buffer[PRINTK_BUFFER_SIZE];
static size_t log_head;
static size_t log_tail;
static size_t log_size;
static bool   log_wrapped;

/* ── Pluggable output backends ────────────────────────────────────────────── */

static void (*output_fn)(char c);
static void (*color_output_fn)(char c, color_t color);
static void (*console_write)(const char *buf, size_t len, void *data);
static void  *console_user_data;

/* ── State ────────────────────────────────────────────────────────────────── */

static spinlock_t printk_lock = SPINLOCK_INIT("printk");
static bool       printk_at_line_start = true;
static color_t    current_color = COLOR_WHITE;

/* ── Output helpers ───────────────────────────────────────────────────────── */

/**
 * @brief Set the single-character output function (early boot path).
 */
void printk_set_output(void (*fn)(char c))
{
	output_fn = fn;
}

/**
 * @brief Set the color-aware single-character output function.
 */
void printk_set_color_output(void (*fn)(char c, color_t color))
{
	color_output_fn = fn;
}

/**
 * @brief Map a log level to its display colour.
 */
static color_t printk_level_color(int level)
{
	switch (level)
	{
	case 0:
	case 1:
	case 2:
	case 3:
		return COLOR_RED;
	case 4:
		return COLOR_YELLOW;
	case 5:
	case 6:
		return COLOR_BLUE;
	case 7:
		return COLOR_WHITE;
	case 8:
		return COLOR_GREEN;
	default:
		return COLOR_WHITE;
	}
}

/**
 * @brief Get the label string for a given log level.
 *
 * @return Pointer to a static string, or NULL for unrecognised levels.
 */
static const char *printk_level_label(int level)
{
	switch (level)
	{
	case 0:
	case 1:
	case 2:
	case 3:
		return "[ERROR] ";
	case 4:
		return "[WARN]  ";
	case 5:
	case 6:
		return "[INFO]  ";
	case 7:
		return "[DEBUG] ";
	case 8:
		return "[ OK ]  ";
	default:
		return NULL;
	}
}

/* ── Token-based colourisation ────────────────────────────────────────────── */

struct printk_token {
	const char *text;
	size_t      len;
	color_t     color;
};

static const struct printk_token tokens[] = {
	{"[ OK ]",   6, COLOR_GREEN},
	{"[OK]",     4, COLOR_GREEN},
	{"[ERROR]",  7, COLOR_RED},
	{"[WARN]",   6, COLOR_YELLOW},
	{"[INFO]",   6, COLOR_BLUE},
	{"[DEBUG]",  7, COLOR_WHITE},
};

static int printk_token_len(const char *str, size_t len, color_t *color)
{
	for (size_t i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
	{
		if (len >= tokens[i].len &&
		    strncmp(str, tokens[i].text, tokens[i].len) == 0)
		{
			*color = tokens[i].color;
			return (int)tokens[i].len;
		}
	}
	return 0;
}

/* ── Ring-buffer management ───────────────────────────────────────────────── */

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
			log_wrapped = true;
			log_tail = (log_tail + 1) % PRINTK_BUFFER_SIZE;
		}
	}
}

/* ── Output plumbing ──────────────────────────────────────────────────────── */

static void output_plain_string(const char *str, size_t len, color_t color)
{
	log_buffer_append(str, len);

	if (console_write)
	{
		console_write(str, len, console_user_data);
	}
	else if (color_output_fn)
	{
		for (size_t i = 0; i < len; i++)
			color_output_fn(str[i], color);
	}
	else if (output_fn)
	{
		for (size_t i = 0; i < len; i++)
			output_fn(str[i]);
	}

	for (size_t i = 0; i < len; i++)
		printk_at_line_start = (str[i] == '\n');
}

static void output_level_label(const char *str, size_t len, color_t color)
{
	if (len < 2)
	{
		output_plain_string(str, len, COLOR_WHITE);
		return;
	}

	/* Colour the inside of [LEVEL], leaving brackets white. */
	output_plain_string(str, 1, COLOR_WHITE);
	output_plain_string(str + 1, len - 2, color);
	output_plain_string(str + len - 1, 1, COLOR_WHITE);
}

static void output_string_color(const char *str, size_t len, color_t color)
{
	size_t pos = 0;

	while (pos < len)
	{
		color_t token_color;
		int token_len = printk_token_len(str + pos, len - pos, &token_color);

		if (token_len > 0)
		{
			output_level_label(str + pos, (size_t)token_len, token_color);
			pos += (size_t)token_len;
			continue;
		}

		output_plain_string(str + pos, 1, color);
		pos++;
	}
}

static void output_string(const char *str, size_t len)
{
	output_string_color(str, len, current_color);
}

/* ── Console registration ─────────────────────────────────────────────────── */

/**
 * @brief Register a fully-capable console backend.
 *
 * Once registered, all buffered log contents are flushed to the new
 * console. The console replaces the simpler early-output backends.
 *
 * @param write_fn  Callback invoked with each log chunk.
 * @param user_data Opaque pointer passed to the callback.
 */
void printk_register_console(void (*write_fn)(const char *, size_t, void *),
			     void *user_data)
{
	console_write       = write_fn;
	console_user_data   = user_data;

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

/**
 * @brief Unregister the console backend, reverting to early output.
 */
void printk_unregister_console(void)
{
	console_write       = NULL;
	console_user_data   = NULL;
}

/* ─── Log buffer access (for crash dumps, /proc/kmsg, etc.) ──────────────── */

size_t printk_get_log(char *dest, size_t max_len)
{
	if (!dest || max_len == 0)
		return 0;

	size_t to_copy = (log_size < max_len) ? log_size : max_len;
	size_t copied = 0;
	size_t pos = log_tail;

	while (copied < to_copy)
	{
		dest[copied++] = log_buffer[pos];
		pos = (pos + 1) % PRINTK_BUFFER_SIZE;
	}
	return copied;
}

void printk_clear_log(void)
{
	log_head    = 0;
	log_tail    = 0;
	log_size    = 0;
	log_wrapped = false;
}

size_t printk_log_size(void)
{
	return log_size;
}

/* ── Formatting helpers ───────────────────────────────────────────────────── */

static void printk_pad(char c, int count)
{
	char buf[256];

	while (count > 0)
	{
		int chunk = (count > (int)sizeof(buf)) ? (int)sizeof(buf) : count;
		memset(buf, c, (size_t)chunk);
		output_string(buf, (size_t)chunk);
		count -= chunk;
	}
}

enum {
	FLAG_LEFT_ADJUST  = 1U << 0,
	FLAG_SHOW_SIGN    = 1U << 1,
	FLAG_SPACE        = 1U << 2,
	FLAG_ALT_FORM     = 1U << 3,
	FLAG_ZERO_PAD     = 1U << 4,
};

enum length_mod {
	LEN_NONE,
	LEN_HH,
	LEN_H,
	LEN_L,
	LEN_LL,
	LEN_Z,
	LEN_T,
	LEN_J,
};

static const char *parse_flags(const char *fmt, unsigned int *flags)
{
	*flags = 0;
	while (*fmt)
	{
		switch (*fmt)
		{
		case '-': *flags |= FLAG_LEFT_ADJUST; fmt++; break;
		case '+': *flags |= FLAG_SHOW_SIGN;   fmt++; break;
		case ' ': *flags |= FLAG_SPACE;       fmt++; break;
		case '#': *flags |= FLAG_ALT_FORM;    fmt++; break;
		case '0': *flags |= FLAG_ZERO_PAD;    fmt++; break;
		default:  return fmt;
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
		*width = *width * 10 + (*fmt++ - '0');
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
		*precision = *precision * 10 + (*fmt++ - '0');
	return fmt;
}

static const char *parse_length(const char *fmt, enum length_mod *length)
{
	*length = LEN_NONE;
	if (*fmt == 'h')
	{
		fmt++;
		*length = (*fmt == 'h') ? (fmt++, LEN_HH) : LEN_H;
	}
	else if (*fmt == 'l')
	{
		fmt++;
		*length = (*fmt == 'l') ? (fmt++, LEN_LL) : LEN_L;
	}
	else if (*fmt == 'z')  { *length = LEN_Z; fmt++; }
	else if (*fmt == 't')  { *length = LEN_T; fmt++; }
	else if (*fmt == 'j')  { *length = LEN_J; fmt++; }
	return fmt;
}

static char *format_uint(uintmax_t num, char *buf, int base,
			 bool uppercase, int precision)
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
		*--ptr = digits[num % (unsigned int)base];
		num /= (unsigned int)base;
	}

	int len = (int)((buf + 64) - ptr);
	while (len < precision)
	{
		*--ptr = '0';
		len++;
	}
	return ptr;
}

static char *format_int(intmax_t num, char *buf, bool *is_negative)
{
	*is_negative = (num < 0);
	if (*is_negative)
		num = -num;
	return format_uint((uintmax_t)num, buf, 10, false, 0);
}

static void output_formatted(const char *str, int str_len, int width,
			     unsigned int flags, char pad_char,
			     const char *prefix, int prefix_len)
{
	int total_len = str_len + prefix_len;
	int padding = (width > total_len) ? (width - total_len) : 0;

	if (flags & FLAG_LEFT_ADJUST)
	{
		if (prefix_len > 0)
			output_string(prefix, (size_t)prefix_len);
		output_string(str, (size_t)str_len);
		printk_pad(' ', padding);
	}
	else if ((flags & FLAG_ZERO_PAD) && !(flags & FLAG_LEFT_ADJUST))
	{
		if (prefix_len > 0)
			output_string(prefix, (size_t)prefix_len);
		printk_pad('0', padding);
		output_string(str, (size_t)str_len);
	}
	else
	{
		printk_pad(pad_char, padding);
		if (prefix_len > 0)
			output_string(prefix, (size_t)prefix_len);
		output_string(str, (size_t)str_len);
	}
}

/* ── Core printk engine ───────────────────────────────────────────────────── */

/**
 * @brief Format and output a log message.
 *
 * Parses a KERN_* level prefix, applies level-based colour/label, then
 * processes the printf(3)-style format string.
 *
 * @param fmt  Format string, optionally prefixed with "<N>" for log level.
 * @param args Variable argument list.
 */
void vprintk(const char *fmt, va_list args)
{
	int level = -1;

	if (fmt[0] == '<' && fmt[1] >= '0' && fmt[1] <= '8' && fmt[2] == '>')
		level = fmt[1] - '0';

	bool is_critical = (level >= 0 && level <= 2);

	if (level >= 0)
		fmt += 3;

	while (*fmt == '\n' || *fmt == '\r')
	{
		char c = *fmt++;
		output_string(&c, 1);
	}

	current_color = COLOR_WHITE;

	const char *level_label = printk_level_label(level);
	if (level_label && printk_at_line_start)
	{
		size_t label_len = strlen(level_label);
		output_level_label(level_label, label_len - 1,
				   printk_level_color(level));
		output_plain_string(level_label + label_len - 1, 1, COLOR_WHITE);
	}

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

		unsigned int flags;
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
			case LEN_HH: ival = (signed char)va_arg(args_copy, int);       break;
			case LEN_H:  ival = (short)va_arg(args_copy, int);             break;
			case LEN_L:  ival = va_arg(args_copy, long);                   break;
			case LEN_LL: ival = va_arg(args_copy, long long);              break;
			case LEN_Z:  ival = (intmax_t)va_arg(args_copy, size_t);       break;
			case LEN_T:  ival = (intmax_t)va_arg(args_copy, ptrdiff_t);    break;
			case LEN_J:  ival = va_arg(args_copy, intmax_t);               break;
			default:     ival = va_arg(args_copy, int);                    break;
			}
			str = format_int(ival, buf, &is_negative);
			str_len = (int)strlen(str);
			if (is_negative)
				prefix[prefix_len++] = '-';
			else if (flags & FLAG_SHOW_SIGN)
				prefix[prefix_len++] = '+';
			else if (flags & FLAG_SPACE)
				prefix[prefix_len++] = ' ';
			if (precision >= 0)
				flags &= ~FLAG_ZERO_PAD;
			output_formatted(str, str_len, width, flags, ' ',
					 prefix, prefix_len);
			break;

		case 'u':
		case 'o':
		case 'x':
		case 'X':
			switch (length)
			{
			case LEN_HH: uval = (unsigned char)va_arg(args_copy, unsigned int);        break;
			case LEN_H:  uval = (unsigned short)va_arg(args_copy, unsigned int);       break;
			case LEN_L:  uval = va_arg(args_copy, unsigned long);                      break;
			case LEN_LL: uval = va_arg(args_copy, unsigned long long);                break;
			case LEN_Z:  uval = va_arg(args_copy, size_t);                             break;
			case LEN_T:  uval = (uintmax_t)va_arg(args_copy, ptrdiff_t);               break;
			case LEN_J:  uval = va_arg(args_copy, uintmax_t);                          break;
			default:     uval = va_arg(args_copy, unsigned int);                       break;
			}
			{
				int base = (specifier == 'o') ? 8
				         : (specifier == 'u') ? 10 : 16;
				str = format_uint(uval, buf, base,
						 specifier == 'X', precision);
				str_len = (int)strlen(str);
				if ((flags & FLAG_ALT_FORM) && uval != 0)
				{
					if (specifier == 'o' && str[0] != '0')
						prefix[prefix_len++] = '0';
					else if (specifier == 'x' || specifier == 'X')
					{
						prefix[prefix_len++] = '0';
						prefix[prefix_len++] = specifier;
					}
				}
				if (precision >= 0)
					flags &= ~FLAG_ZERO_PAD;
				output_formatted(str, str_len, width, flags, ' ',
						 prefix, prefix_len);
			}
			break;

		case 'p':
			uval = (uintptr_t)va_arg(args_copy, void *);
			str = format_uint(uval, buf, 16, false, sizeof(void *) * 2);
			str_len = (int)strlen(str);
			prefix[prefix_len++] = '0';
			prefix[prefix_len++] = 'x';
			output_formatted(str, str_len, width, flags, ' ',
					 prefix, prefix_len);
			break;

		case 's':
			{
				const char *s = va_arg(args_copy, const char *);
				if (!s)
					s = "(null)";
				str_len = (int)strlen(s);
				if (precision >= 0 && str_len > precision)
					str_len = precision;
				output_formatted(s, str_len, width, flags, ' ',
						 NULL, 0);
			}
			break;

		case 'c':
			{
				char c = (char)va_arg(args_copy, int);
				buf[0] = c;
				output_formatted(buf, 1, width, flags, ' ',
						 NULL, 0);
			}
			break;

		case 'n':
			break;

		default:
			{
				char tmp[2] = { '%', specifier };
				output_string(tmp, 2);
			}
			break;
		}
	}

	va_end(args_copy);
	current_color = COLOR_WHITE;

	if (is_critical)
	{
		/* Spin briefly to let the serial port drain. */
		for (volatile int i = 0; i < 10000000; i++)
			;
	}
}

/**
 * @brief Kernel formatted print (printf-compatible).
 *
 * Acquires the printk spinlock, formats the message via vprintk(),
 * then releases the lock.
 *
 * @param fmt Format string with optional KERN_* level prefix.
 */
void printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	spinlock_acquire(&printk_lock);
	vprintk(fmt, args);
	spinlock_release(&printk_lock);

	va_end(args);
}
