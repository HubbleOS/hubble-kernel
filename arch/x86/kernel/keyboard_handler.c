#include <printk.h>
#include <gdt/interrupt.h>

void keyboard_handler(registers_t *regs)
{
	// Читаем скан-код с порта 0x60
	uint8_t scancode;
	asm volatile("inb $0x60, %0" : "=a"(scancode));

	printk("Keyboard scancode: 0x%x\n", scancode);
}
