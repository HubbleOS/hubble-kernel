#include <stddef.h>
#include <stdint.h>

#define PERIPHERAL_BASE 0x3F000000UL
#define GPIO_BASE (PERIPHERAL_BASE + 0x200000UL)
#define UART0_BASE (PERIPHERAL_BASE + 0x201000UL)

#define GPFSEL1 (GPIO_BASE + 0x04)
#define GPSET0 (GPIO_BASE + 0x1c)
#define GPCLR0 (GPIO_BASE + 0x28)
#define GPPUD (GPIO_BASE + 0x94)
#define GPPUDCLK0 (GPIO_BASE + 0x98)

#define GPIO_FUNC_OUTPUT 1u
#define ACT_LED_GPIO 29u
#define ACT_LED_ACTIVE_HIGH 1u
#define UART0_IBRD_9600 312u
#define UART0_FBRD_9600 32u

#define LCD_DC_GPIO 18u
#define LCD_MOSI_GPIO 19u
#define LCD_RST_GPIO 22u
#define LCD_SCLK_GPIO 23u
#define LCD_CS_GPIO 24u

#define LCD_WIDTH 480u
#define LCD_HEIGHT 320u

#define UART0_DR (UART0_BASE + 0x00)
#define UART0_FR (UART0_BASE + 0x18)
#define UART0_IBRD (UART0_BASE + 0x24)
#define UART0_FBRD (UART0_BASE + 0x28)
#define UART0_LCRH (UART0_BASE + 0x2c)
#define UART0_CR (UART0_BASE + 0x30)
#define UART0_IMSC (UART0_BASE + 0x38)
#define UART0_ICR (UART0_BASE + 0x44)

static inline void mmio_write(uintptr_t reg, uint32_t value) {
  *(volatile uint32_t *)reg = value;
}

static inline uint32_t mmio_read(uintptr_t reg) {
  return *(volatile uint32_t *)reg;
}

static void uart_puts(const char *s);

static void delay(unsigned int count) {
  while (count--) {
    __asm__ volatile("nop");
  }
}

static void gpio_set_function(unsigned int pin, unsigned int function) {
  uintptr_t reg = GPIO_BASE + ((pin / 10u) * 4u);
  unsigned int shift = (pin % 10u) * 3u;
  uint32_t selector = mmio_read(reg);

  selector &= ~(7u << shift);
  selector |= (function & 7u) << shift;
  mmio_write(reg, selector);
}

static void gpio_write(unsigned int pin, unsigned int high) {
  if (high) {
    mmio_write(GPSET0, 1u << pin);
  } else {
    mmio_write(GPCLR0, 1u << pin);
  }
}

static void lcd_spi_delay(void) {
  __asm__ volatile("nop");
  __asm__ volatile("nop");
  __asm__ volatile("nop");
}

static void lcd_spi_write_bit(unsigned int bit) {
  gpio_write(LCD_MOSI_GPIO, bit);
  lcd_spi_delay();
  gpio_write(LCD_SCLK_GPIO, 1);
  lcd_spi_delay();
  gpio_write(LCD_SCLK_GPIO, 0);
}

static void lcd_spi_write_byte(uint8_t value) {
  for (int i = 7; i >= 0; --i) {
    lcd_spi_write_bit((value >> i) & 1u);
  }
}

static void lcd_write_cmd(uint8_t cmd) {
  gpio_write(LCD_DC_GPIO, 0);
  gpio_write(LCD_CS_GPIO, 0);
  lcd_spi_write_byte(cmd);
  gpio_write(LCD_CS_GPIO, 1);
}

static void lcd_write_data(uint8_t data) {
  gpio_write(LCD_DC_GPIO, 1);
  gpio_write(LCD_CS_GPIO, 0);
  lcd_spi_write_byte(data);
  gpio_write(LCD_CS_GPIO, 1);
}

static void lcd_write_data16(uint16_t data) {
  lcd_write_data((uint8_t)(data >> 8));
  lcd_write_data((uint8_t)(data & 0xff));
}

static void lcd_write_cmd_bytes(uint8_t cmd, const uint8_t *data, size_t len) {
  gpio_write(LCD_DC_GPIO, 0);
  gpio_write(LCD_CS_GPIO, 0);
  lcd_spi_write_byte(cmd);
  gpio_write(LCD_DC_GPIO, 1);

  for (size_t i = 0; i < len; ++i) {
    lcd_spi_write_byte(data[i]);
  }

  gpio_write(LCD_CS_GPIO, 1);
}

static void lcd_begin_data(void) {
  gpio_write(LCD_DC_GPIO, 1);
  gpio_write(LCD_CS_GPIO, 0);
}

static void lcd_end_data(void) { gpio_write(LCD_CS_GPIO, 1); }

static void lcd_write_data_stream(uint8_t data) { lcd_spi_write_byte(data); }

static void lcd_reset(void) {
  gpio_write(LCD_RST_GPIO, 1);
  delay(100000);
  gpio_write(LCD_RST_GPIO, 0);
  delay(100000);
  gpio_write(LCD_RST_GPIO, 1);
  delay(200000);
}

static void lcd_gpio_init(void) {
  gpio_set_function(LCD_DC_GPIO, GPIO_FUNC_OUTPUT);
  gpio_set_function(LCD_MOSI_GPIO, GPIO_FUNC_OUTPUT);
  gpio_set_function(LCD_RST_GPIO, GPIO_FUNC_OUTPUT);
  gpio_set_function(LCD_SCLK_GPIO, GPIO_FUNC_OUTPUT);
  gpio_set_function(LCD_CS_GPIO, GPIO_FUNC_OUTPUT);

  gpio_write(LCD_CS_GPIO, 1);
  gpio_write(LCD_SCLK_GPIO, 0);
  gpio_write(LCD_DC_GPIO, 1);
  gpio_write(LCD_RST_GPIO, 1);
}

static void lcd_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1,
                                uint16_t y1) {
  uint8_t col[4] = {
      (uint8_t)(x0 >> 8),
      (uint8_t)x0,
      (uint8_t)(x1 >> 8),
      (uint8_t)x1,
  };
  uint8_t row[4] = {
      (uint8_t)(y0 >> 8),
      (uint8_t)y0,
      (uint8_t)(y1 >> 8),
      (uint8_t)y1,
  };

  lcd_write_cmd_bytes(0x2A, col, sizeof(col));
  lcd_write_cmd_bytes(0x2B, row, sizeof(row));
  lcd_write_cmd(0x2C);
}

static void lcd_fill_color(uint16_t color, size_t pixels) {
  uint8_t hi = (uint8_t)(color >> 8);
  uint8_t lo = (uint8_t)(color & 0xff);

  lcd_begin_data();

  for (size_t i = 0; i < pixels; ++i) {
    lcd_write_data_stream(hi);
    lcd_write_data_stream(lo);
  }

  lcd_end_data();
}

static void lcd_ili9486_init(void) {
  lcd_gpio_init();
  lcd_reset();

  uart_puts("lcd: reset done\n");
  lcd_write_cmd(0x01); /* software reset */
  delay(200000);
  uart_puts("lcd: sw reset\n");

  lcd_write_cmd(0x11); /* sleep out */
  delay(200000);
  uart_puts("lcd: sleep out\n");

  {
    uint8_t pixel_format[] = {0x55};
    lcd_write_cmd_bytes(0x3A, pixel_format, sizeof(pixel_format));
  }
  uart_puts("lcd: pixel format\n");

  {
    uint8_t madctl[] = {0x48};
    lcd_write_cmd_bytes(0x36, madctl, sizeof(madctl));
  }
  uart_puts("lcd: madctl\n");

  uart_puts("lcd: B0\n");
  {
    uint8_t data[] = {0x00, 0x10};
    lcd_write_cmd_bytes(0xB0, data, sizeof(data));
  }
  uart_puts("lcd: B0 ok\n");

  uart_puts("lcd: B6\n");
  {
    uint8_t data[] = {0x02, 0x22};
    lcd_write_cmd_bytes(0xB6, data, sizeof(data));
  }
  uart_puts("lcd: B6 ok\n");

  uart_puts("lcd: C0\n");
  {
    uint8_t data[] = {0x0A, 0x02};
    lcd_write_cmd_bytes(0xC0, data, sizeof(data));
  }
  uart_puts("lcd: C0 ok\n");

  uart_puts("lcd: C1\n");
  {
    uint8_t data[] = {0x41};
    lcd_write_cmd_bytes(0xC1, data, sizeof(data));
  }
  uart_puts("lcd: C1 ok\n");

  uart_puts("lcd: C5\n");
  {
    uint8_t data[] = {0x00, 0x1A, 0x80};
    lcd_write_cmd_bytes(0xC5, data, sizeof(data));
  }
  uart_puts("lcd: C5 ok\n");

  lcd_write_cmd(0x20); /* display inversion off */
  lcd_write_cmd(0x29); /* display on */
  delay(100000);
  uart_puts("lcd: display on\n");

  lcd_set_addr_window(0, 0, LCD_WIDTH - 1u, LCD_HEIGHT - 1u);
  lcd_fill_color(0x0000, LCD_WIDTH * LCD_HEIGHT);
}

static void act_led_init(void) {
  gpio_set_function(ACT_LED_GPIO, GPIO_FUNC_OUTPUT);
  gpio_write(ACT_LED_GPIO, !ACT_LED_ACTIVE_HIGH);
}

static void act_led_write(unsigned int on) {
  gpio_write(ACT_LED_GPIO, ACT_LED_ACTIVE_HIGH ? on : !on);
}

static void uart_init(void) {
  uint32_t selector;

  mmio_write(UART0_CR, 0);

  selector = mmio_read(GPFSEL1);
  selector &= ~((7u << 12) | (7u << 15));
  selector |= (4u << 12) | (4u << 15);
  mmio_write(GPFSEL1, selector);

  mmio_write(GPPUD, 0);
  delay(150);
  mmio_write(GPPUDCLK0, (1u << 14) | (1u << 15));
  delay(150);
  mmio_write(GPPUDCLK0, 0);

  mmio_write(UART0_ICR, 0x7ff);
  mmio_write(UART0_IBRD, UART0_IBRD_9600);
  mmio_write(UART0_FBRD, UART0_FBRD_9600);
  mmio_write(UART0_LCRH, (3u << 5) | (1u << 4));
  mmio_write(UART0_IMSC, 0);
  mmio_write(UART0_CR, (1u << 9) | (1u << 8) | 1u);
}

static void uart_putc(char c) {
  if (c == '\n') {
    uart_putc('\r');
  }

  while ((mmio_read(UART0_FR) & 0x20) != 0) {
    __asm__ volatile("nop");
  }

  mmio_write(UART0_DR, (uint32_t)c);
}

static void uart_puts(const char *s) {
  while (*s != '\0') {
    uart_putc(*s++);
  }
}

void kernel_main(void) {
  act_led_init();
  // uart_init();
  // uart_puts("boot: uart ready\n");
  // lcd_ili9486_init();

  lcd_write_cmd(0x10);
  delay(1000000);

  // uart_puts("\nRaspberry Pi 3B+ bare-metal kernel8.img\n");
  //     uart_puts("UART ready. LCD ILI9486 init done.\n");

  for (;;) {
    act_led_write(1);
    // uart_puts("LCD init alive\n");
    delay(50000000);

    act_led_write(0);
    // uart_puts("LCD init alive\n");
    delay(50000000);
  }
}
