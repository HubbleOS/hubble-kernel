// #include <stddef.h>
// #include <stdint.h>

// static uint32_t MMIO_BASE;

// // The MMIO area base address, depends on board type
// static inline void mmio_init(int raspi)
// {
// 	switch (raspi)
// 	{
// 	case 2:
// 	case 3:
// 		MMIO_BASE = 0x3F000000;
// 		break; // for raspi2 & 3
// 	case 4:
// 		MMIO_BASE = 0xFE000000;
// 		break; // for raspi4
// 	default:
// 		MMIO_BASE = 0x20000000;
// 		break; // for raspi1, raspi zero etc.
// 	}
// }

// // Memory-Mapped I/O output
// static inline void mmio_write(uint32_t reg, uint32_t data)
// {
// 	*(volatile uint32_t *)(MMIO_BASE + reg) = data;
// }

// // Memory-Mapped I/O input
// static inline uint32_t mmio_read(uint32_t reg)
// {
// 	return *(volatile uint32_t *)(MMIO_BASE + reg);
// }

// // Loop <delay> times in a way that the compiler won't optimize away
// static inline void delay(int32_t count)
// {
// 	asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
// 		     : "=r"(count) : [count] "0"(count) : "cc");
// }

// enum
// {
// 	// The offsets for reach register.
// 	GPIO_BASE = 0x200000,

// 	// Controls actuation of pull up/down to ALL GPIO pins.
// 	GPPUD = (GPIO_BASE + 0x94),

// 	// Controls actuation of pull up/down for specific GPIO pin.
// 	GPPUDCLK0 = (GPIO_BASE + 0x98),

// 	// The base address for UART.
// 	UART0_BASE = (GPIO_BASE + 0x1000), // for raspi4 0xFE201000, raspi2 & 3 0x3F201000, and 0x20201000 for raspi1

// 	// The offsets for reach register for the UART.
// 	UART0_DR = (UART0_BASE + 0x00),
// 	UART0_RSRECR = (UART0_BASE + 0x04),
// 	UART0_FR = (UART0_BASE + 0x18),
// 	UART0_ILPR = (UART0_BASE + 0x20),
// 	UART0_IBRD = (UART0_BASE + 0x24),
// 	UART0_FBRD = (UART0_BASE + 0x28),
// 	UART0_LCRH = (UART0_BASE + 0x2C),
// 	UART0_CR = (UART0_BASE + 0x30),
// 	UART0_IFLS = (UART0_BASE + 0x34),
// 	UART0_IMSC = (UART0_BASE + 0x38),
// 	UART0_RIS = (UART0_BASE + 0x3C),
// 	UART0_MIS = (UART0_BASE + 0x40),
// 	UART0_ICR = (UART0_BASE + 0x44),
// 	UART0_DMACR = (UART0_BASE + 0x48),
// 	UART0_ITCR = (UART0_BASE + 0x80),
// 	UART0_ITIP = (UART0_BASE + 0x84),
// 	UART0_ITOP = (UART0_BASE + 0x88),
// 	UART0_TDR = (UART0_BASE + 0x8C),

// 	// The offsets for Mailbox registers
// 	MBOX_BASE = 0xB880,
// 	MBOX_READ = (MBOX_BASE + 0x00),
// 	MBOX_STATUS = (MBOX_BASE + 0x18),
// 	MBOX_WRITE = (MBOX_BASE + 0x20)
// };

// // A Mailbox message with set clock rate of PL011 to 3MHz tag
// volatile unsigned int __attribute__((aligned(16))) mbox[9] = {
//     9 * 4, 0, 0x38002, 12, 8, 2, 3000000, 0, 0};

// void uart_init(int raspi)
// {
// 	mmio_init(raspi);

// 	// Disable UART0.
// 	mmio_write(UART0_CR, 0x00000000);
// 	// Setup the GPIO pin 14 && 15.

// 	// Disable pull up/down for all GPIO pins & delay for 150 cycles.
// 	mmio_write(GPPUD, 0x00000000);
// 	delay(150);

// 	// Disable pull up/down for pin 14,15 & delay for 150 cycles.
// 	mmio_write(GPPUDCLK0, (1 << 14) | (1 << 15));
// 	delay(150);

// 	// Write 0 to GPPUDCLK0 to make it take effect.
// 	mmio_write(GPPUDCLK0, 0x00000000);

// 	// Clear pending interrupts.
// 	mmio_write(UART0_ICR, 0x7FF);

// 	// Set integer & fractional part of baud rate.
// 	// Divider = UART_CLOCK/(16 * Baud)
// 	// Fraction part register = (Fractional part * 64) + 0.5
// 	// Baud = 115200.

// 	// For Raspi3 and 4 the UART_CLOCK is system-clock dependent by default.
// 	// Set it to 3Mhz so that we can consistently set the baud rate
// 	if (raspi >= 3)
// 	{
// 		// UART_CLOCK = 30000000;
// 		unsigned int r = (((unsigned int)(&mbox) & ~0xF) | 8);
// 		// wait until we can talk to the VC
// 		while (mmio_read(MBOX_STATUS) & 0x80000000)
// 		{
// 		}
// 		// send our message to property channel and wait for the response
// 		mmio_write(MBOX_WRITE, r);
// 		while ((mmio_read(MBOX_STATUS) & 0x40000000) || mmio_read(MBOX_READ) != r)
// 			;
// 	}

// 	// Divider = 3000000 / (16 * 115200) = 1.627 = ~1.
// 	mmio_write(UART0_IBRD, 1);
// 	// Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
// 	mmio_write(UART0_FBRD, 40);

// 	// Enable FIFO & 8 bit data transmission (1 stop bit, no parity).
// 	mmio_write(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));

// 	// Mask all interrupts.
// 	mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) |
// 				   (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));

// 	// Enable UART0, receive & transfer part of UART.
// 	mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));
// }

// void uart_putc(unsigned char c)
// {
// 	// Wait for UART to become ready to transmit.
// 	while (mmio_read(UART0_FR) & (1 << 5))
// 	{
// 	}
// 	mmio_write(UART0_DR, c);
// }

// unsigned char uart_getc()
// {
// 	// Wait for UART to have received something.
// 	while (mmio_read(UART0_FR) & (1 << 4))
// 	{
// 	}
// 	return mmio_read(UART0_DR);
// }

// void uart_puts(const char *str)
// {
// 	for (size_t i = 0; str[i] != '\0'; i++)
// 		uart_putc((unsigned char)str[i]);
// }

// // ########

// #if defined(__cplusplus)
// extern "C" /* Use C linkage for kernel_main. */
// #endif

// #ifdef AARCH64
//     // arguments for AArch64
//     void kernel_main(uint64_t dtb_ptr32, uint64_t x1, uint64_t x2, uint64_t x3)
// #else
// // arguments for AArch32
// void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags)
// #endif
// {
// 	uart_init(3);

// 	uart_puts("Hello, kernel World!\r\n");

// 	while (1)
// 		uart_putc(uart_getc());
// }

#include <stddef.h>
#include <stdint.h>

static uint32_t MMIO_BASE;

static inline void mmio_init(int raspi)
{
	switch (raspi)
	{
	case 2:
	case 3:
		MMIO_BASE = 0x3F000000;
		break;
	case 4:
		MMIO_BASE = 0xFE000000;
		break;
	default:
		MMIO_BASE = 0x20000000;
		break;
	}
}

static inline void mmio_write(uint32_t reg, uint32_t data)
{
	*(volatile uint32_t *)(MMIO_BASE + reg) = data;
}

static inline uint32_t mmio_read(uint32_t reg)
{
	return *(volatile uint32_t *)(MMIO_BASE + reg);
}

static inline void delay(int32_t count)
{
	asm volatile("__delay_%=: subs %[count], %[count], #1; bne __delay_%=\n"
		     : "=r"(count) : [count] "0"(count) : "cc");
}

enum
{
	GPIO_BASE = 0x200000,
	GPPUD = (GPIO_BASE + 0x94),
	GPPUDCLK0 = (GPIO_BASE + 0x98),
	UART0_BASE = (GPIO_BASE + 0x1000),
	UART0_DR = (UART0_BASE + 0x00),
	UART0_FR = (UART0_BASE + 0x18),
	UART0_IBRD = (UART0_BASE + 0x24),
	UART0_FBRD = (UART0_BASE + 0x28),
	UART0_LCRH = (UART0_BASE + 0x2C),
	UART0_CR = (UART0_BASE + 0x30),
	UART0_IMSC = (UART0_BASE + 0x38),
	UART0_ICR = (UART0_BASE + 0x44),

	MBOX_BASE = 0xB880,
	MBOX_READ = (MBOX_BASE + 0x00),
	MBOX_STATUS = (MBOX_BASE + 0x18),
	MBOX_WRITE = (MBOX_BASE + 0x20),
};

// ── Mailbox ──────────────────────────────────────────────────────────────────

static void mbox_send(volatile unsigned int *msg)
{
	unsigned int r = (((unsigned int)(uintptr_t)msg & ~0xF) | 8);
	while (mmio_read(MBOX_STATUS) & 0x80000000)
	{
	}
	mmio_write(MBOX_WRITE, r);
	while ((mmio_read(MBOX_STATUS) & 0x40000000) || mmio_read(MBOX_READ) != r)
	{
	}
}

// ── UART ─────────────────────────────────────────────────────────────────────

volatile unsigned int __attribute__((aligned(16))) uart_mbox[9] = {
    9 * 4, 0, 0x38002, 12, 8, 2, 3000000, 0, 0};

void uart_init(int raspi)
{
	mmio_init(raspi);
	mmio_write(UART0_CR, 0);
	mmio_write(GPPUD, 0);
	delay(150);
	mmio_write(GPPUDCLK0, (1 << 14) | (1 << 15));
	delay(150);
	mmio_write(GPPUDCLK0, 0);
	mmio_write(UART0_ICR, 0x7FF);

	if (raspi >= 3)
		mbox_send(uart_mbox);

	mmio_write(UART0_IBRD, 1);
	mmio_write(UART0_FBRD, 40);
	mmio_write(UART0_LCRH, (1 << 4) | (1 << 5) | (1 << 6));
	mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10));
	mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));
}

void uart_putc(unsigned char c)
{
	while (mmio_read(UART0_FR) & (1 << 5))
	{
	}
	mmio_write(UART0_DR, c);
}

unsigned char uart_getc()
{
	while (mmio_read(UART0_FR) & (1 << 4))
	{
	}
	return mmio_read(UART0_DR);
}

void uart_puts(const char *str)
{
	for (size_t i = 0; str[i]; i++)
		uart_putc((unsigned char)str[i]);
}

// ── Framebuffer ───────────────────────────────────────────────────────────────

#define FB_WIDTH 1024
#define FB_HEIGHT 768

volatile unsigned int __attribute__((aligned(16))) fb_mbox[36] = {
    36 * 4, 0,
    0x48003, 8, 0, FB_WIDTH, FB_HEIGHT, // set physical (display) size
    0x48004, 8, 0, FB_WIDTH, FB_HEIGHT, // set virtual (buffer)  size
    0x48005, 4, 0, 32,			// set colour depth: 32 bpp
    0x48006, 4, 0, 0,			// set pixel order: RGB
    0x40001, 8, 0, 4096, 0,		// allocate framebuffer → [ptr, size]
    0x40008, 4, 0, FB_WIDTH * 4,	// set pitch (bytes per row)
    0};

static unsigned char *fb = 0;
static unsigned int fb_pitch = 0;

// виводить uint32 в hex через UART
static void uart_hex(unsigned int n)
{
	uart_puts("0x");
	for (int i = 28; i >= 0; i -= 4)
	{
		unsigned int d = (n >> i) & 0xF;
		uart_putc(d < 10 ? '0' + d : 'A' + d - 10);
	}
	uart_puts("\r\n");
}

// повертає 1 якщо успішно
int fb_init(void)
{
	mbox_send(fb_mbox);

	// дамп всіх 36 слів відповіді
	uart_puts("mbox dump:\r\n");
	for (int i = 0; i < 36; i++)
	{
		uart_puts("[");
		uart_putc('0' + i / 10);
		uart_putc('0' + i % 10);
		uart_puts("] ");
		uart_hex(fb_mbox[i]);
	}

	if (fb_mbox[1] != 0x80000000)
		return 0;
	if (fb_mbox[23] == 0)
		return 0;

	fb = (unsigned char *)(uintptr_t)(fb_mbox[23] & 0x3FFFFFFF);
	fb_pitch = fb_mbox[28];
	return 1;
}

static inline void fb_put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	unsigned int off = (unsigned int)y * fb_pitch + (unsigned int)x * 4;
	fb[off + 0] = r;
	fb[off + 1] = g;
	fb[off + 2] = b;
	fb[off + 3] = 0xFF;
}

void fb_fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b)
{
	for (int j = y; j < y + h; j++)
		for (int i = x; i < x + w; i++)
			fb_put_pixel(i, j, r, g, b);
}

void fb_clear(uint8_t r, uint8_t g, uint8_t b)
{
	fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, r, g, b);
}

// ── kernel_main ───────────────────────────────────────────────────────────────

volatile unsigned int __attribute__((aligned(16))) led_mbox[8] = {
    8 * 4, 0,
    0x00038041, 8, 0, // tag: set GPIO state
    47,		      // pin (Act LED)
    1,		      // 1 = on, 0 = off
    0};

void led_set(int on)
{
	led_mbox[6] = on;
	mbox_send(led_mbox);
}

#if defined(__cplusplus)
extern "C"
#endif
#ifdef AARCH64
    void kernel_main(uint64_t dtb_ptr32, uint64_t x1, uint64_t x2, uint64_t x3)
#else
void kernel_main(uint32_t r0, uint32_t r1, uint32_t atags)
#endif
{
	uart_init(3);
	uart_puts("Hello, kernel World!\r\n");

	if (fb_init())
	{
		uart_puts("Framebuffer OK\r\n");

		fb_clear(0, 0, 0); // чорний фон

		fb_fill_rect(100, 100, 200, 200, 255, 0, 0); // червоний квадрат
		fb_fill_rect(350, 100, 200, 200, 0, 255, 0); // зелений квадрат
		fb_fill_rect(600, 100, 200, 200, 0, 0, 255); // синій квадрат
	}
	else
	{
		uart_puts("Framebuffer FAILED\r\n");
	}

	while (1)
	{
		led_set(1);
		delay(500000);
		led_set(0);
		delay(500000);
	}

	while (1)
		uart_putc(uart_getc());
}
