#include "gdt.h"
#include <string.h>
#include <printk.h>

// ============================================================================
// GDT + TSS Tables
// ============================================================================

// Об'єднана таблиця: 5 GDT дескрипторів + 1 TSS дескриптор (16 байт)
static struct
{
	gdt_entry_t entries[5];	    // 40 bytes
	tss_entry_t tss_descriptor; // 16 bytes
} __attribute__((packed, aligned(16))) gdt_table;

static gdt_ptr_t gdt_ptr;
static tss_t tss;

// Стек для Ring 0
static uint8_t kernel_stack[16384] __attribute__((aligned(16)));

// ============================================================================
// GDT Helper Functions
// ============================================================================

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
	gdt_entry_t *entry = &gdt_table.entries[num];

	entry->base_low = (base & 0xFFFF);
	entry->base_middle = (base >> 16) & 0xFF;
	entry->base_high = (base >> 24) & 0xFF;

	entry->limit_low = (limit & 0xFFFF);
	entry->granularity = (limit >> 16) & 0x0F;
	entry->granularity |= gran & 0xF0;

	entry->access = access;
}

static void tss_set_descriptor(uint64_t base, uint32_t limit)
{
	tss_entry_t *desc = &gdt_table.tss_descriptor;

	desc->limit_low = limit & 0xFFFF;
	desc->base_low = base & 0xFFFF;
	desc->base_middle = (base >> 16) & 0xFF;
	desc->base_high = (base >> 24) & 0xFF;
	desc->base_upper = (base >> 32) & 0xFFFFFFFF;

	desc->access = TSS_ACCESS;
	desc->granularity = 0x00;
	desc->reserved = 0;
}

// ============================================================================
// GDT Initialization
// ============================================================================

void gdt_init(void)
{
	// Розмір всієї таблиці
	gdt_ptr.limit = sizeof(gdt_table) - 1;
	gdt_ptr.base = (uint64_t)&gdt_table;

	printk("GDT Pointer: %p\n", gdt_ptr.base);

	// Очищаємо таблицю
	memset(&gdt_table, 0, sizeof(gdt_table));

	// NULL дескриптор (0x00)
	gdt_set_gate(0, 0, 0, 0, 0);

	// Kernel Code Segment (0x08)
	gdt_set_gate(1, 0, 0xFFFFF,
		     GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM |
			 GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
		     GDT_GRAN_4K | GDT_GRAN_64BIT);

	// Kernel Data Segment (0x10)
	gdt_set_gate(2, 0, 0xFFFFF,
		     GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM |
			 GDT_ACCESS_RW,
		     GDT_GRAN_4K | GDT_GRAN_64BIT);

	// User Data Segment (0x18) - ИЗМЕНЕНО: теперь на позиции 3
	gdt_set_gate(3, 0, 0xFFFFF,
		     GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM |
			 GDT_ACCESS_RW,
		     GDT_GRAN_4K | GDT_GRAN_64BIT);

	// User Code Segment (0x20) - ИЗМЕНЕНО: теперь на позиции 4
	gdt_set_gate(4, 0, 0xFFFFF,
		     GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM |
			 GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
		     GDT_GRAN_4K | GDT_GRAN_64BIT);

	// TSS Descriptor (0x28) - без изменений
	tss_set_descriptor((uint64_t)&tss, sizeof(tss) - 1);

	// Загружаем GDT
	gdt_flush((uint64_t)&gdt_ptr);
}

uint64_t get_gdt_base(void)
{
	printk("GDT Base: %p\n", gdt_ptr.base);
	return gdt_ptr.base;
}

uint16_t get_gdt_limit(void) { return gdt_ptr.limit; }

// ============================================================================
// TSS Initialization
// ============================================================================

void tss_init(void)
{
	// Очищаємо TSS
	memset(&tss, 0, sizeof(tss));

	// Встановлюємо стек для Ring 0
	tss.rsp0 = (uint64_t)(kernel_stack + sizeof(kernel_stack));

	// Загружаем TSS
	tss_flush(GDT_TSS);
}

// ============================================================================
// IDT (решта коду без змін)
// ============================================================================

static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

// Объявления обработчиков прерываний (определены в interrupts.asm)
extern void isr0(void);	 // Division By Zero
extern void isr1(void);	 // Debug
extern void isr2(void);	 // NMI
extern void isr3(void);	 // Breakpoint
extern void isr4(void);	 // Overflow
extern void isr5(void);	 // Bound Range Exceeded
extern void isr6(void);	 // Invalid Opcode
extern void isr7(void);	 // Device Not Available
extern void isr8(void);	 // Double Fault
extern void isr9(void);	 // Coprocessor Segment Overrun
extern void isr10(void); // Invalid TSS
extern void isr11(void); // Segment Not Present
extern void isr12(void); // Stack-Segment Fault
extern void isr13(void); // General Protection Fault
extern void isr14(void); // Page Fault
extern void isr15(void); // Reserved
extern void isr16(void); // x87 Floating-Point Exception
extern void isr17(void); // Alignment Check
extern void isr18(void); // Machine Check
extern void isr19(void); // SIMD Floating-Point Exception
extern void isr20(void); // Virtualization Exception
extern void isr21(void); // Control Protection Exception

// IRQ обработчики (hardware interrupts)
extern void irq0(void);	 // Timer
extern void irq1(void);	 // Keyboard
extern void irq2(void);	 // Cascade
extern void irq3(void);	 // COM2
extern void irq4(void);	 // COM1
extern void irq5(void);	 // LPT2
extern void irq6(void);	 // Floppy
extern void irq7(void);	 // LPT1
extern void irq8(void);	 // RTC
extern void irq9(void);	 // Free
extern void irq10(void); // Free
extern void irq11(void); // Free
extern void irq12(void); // PS/2 Mouse
extern void irq13(void); // FPU
extern void irq14(void); // Primary ATA
extern void irq15(void); // Secondary ATA

// Системный вызов
extern void isr128(void); // System call

// ============================================================================
// IDT Helper Functions
// ============================================================================

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t type_attr)
{
	idt_entries[num].offset_low = handler & 0xFFFF;
	idt_entries[num].offset_mid = (handler >> 16) & 0xFFFF;
	idt_entries[num].offset_high = (handler >> 32) & 0xFFFFFFFF;

	idt_entries[num].selector = selector;
	idt_entries[num].ist = 0; // Не используем IST по умолчанию
	idt_entries[num].type_attr = type_attr;
	idt_entries[num].reserved = 0;
}

// ============================================================================
// IDT Initialization
// ============================================================================

void idt_init(void)
{
	idt_ptr.limit = sizeof(idt_entries) - 1;
	idt_ptr.base = (uint64_t)&idt_entries;

	// Очищаем IDT
	memset(&idt_entries, 0, sizeof(idt_entries));

	// CPU Exceptions (0-21)
	idt_set_gate(0, (uint64_t)isr0, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(1, (uint64_t)isr1, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(2, (uint64_t)isr2, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(3, (uint64_t)isr3, GDT_KERNEL_CODE, IDT_TYPE_TRAP);
	idt_set_gate(4, (uint64_t)isr4, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(5, (uint64_t)isr5, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(6, (uint64_t)isr6, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(7, (uint64_t)isr7, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(8, (uint64_t)isr8, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(9, (uint64_t)isr9, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(10, (uint64_t)isr10, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(11, (uint64_t)isr11, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(12, (uint64_t)isr12, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(13, (uint64_t)isr13, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(14, (uint64_t)isr14, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(15, (uint64_t)isr15, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(16, (uint64_t)isr16, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(17, (uint64_t)isr17, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(18, (uint64_t)isr18, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(19, (uint64_t)isr19, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(20, (uint64_t)isr20, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(21, (uint64_t)isr21, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);

	// IRQs (32-47) - Hardware Interrupts
	idt_set_gate(32, (uint64_t)irq0, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(33, (uint64_t)irq1, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(34, (uint64_t)irq2, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(35, (uint64_t)irq3, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(36, (uint64_t)irq4, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(37, (uint64_t)irq5, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(38, (uint64_t)irq6, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(39, (uint64_t)irq7, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(40, (uint64_t)irq8, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(41, (uint64_t)irq9, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(42, (uint64_t)irq10, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(43, (uint64_t)irq11, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(44, (uint64_t)irq12, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(45, (uint64_t)irq13, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(46, (uint64_t)irq14, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);
	idt_set_gate(47, (uint64_t)irq15, GDT_KERNEL_CODE, IDT_TYPE_INTERRUPT);

	// System Call (0x80 = 128) - доступен из Ring 3
	idt_set_gate(128, (uint64_t)isr128, GDT_KERNEL_CODE, IDT_TYPE_USER);

	// Загружаем IDT
	idt_flush((uint64_t)&idt_ptr);
}
void idt_load()
{
	idt_flush((uint64_t)&idt_ptr);
}
