#pragma once

#include <stdint.h>

// ============================================================================
// GDT (Global Descriptor Table)
// ============================================================================

// GDT Entry структура (8 байт)
typedef struct
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

// TSS Descriptor (16 байт в 64-bit режиме)
typedef struct
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
	uint32_t base_upper;
	uint32_t reserved;
} __attribute__((packed)) tss_entry_t;

// GDT Pointer для LGDT
typedef struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

// ============================================================================
// TSS (Task State Segment)
// ============================================================================

typedef struct
{
	uint32_t reserved0;
	uint64_t rsp0; // Stack pointer для Ring 0
	uint64_t rsp1; // Stack pointer для Ring 1
	uint64_t rsp2; // Stack pointer для Ring 2
	uint64_t reserved1;
	uint64_t ist[7]; // Interrupt Stack Table
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base; // I/O Map Base Address
} __attribute__((packed)) tss_t;

// ============================================================================
// IDT (Interrupt Descriptor Table)
// ============================================================================

// IDT Entry структура (16 байт)
typedef struct
{
	uint16_t offset_low;  // Младшие 16 бит адреса обработчика
	uint16_t selector;    // Селектор сегмента кода
	uint8_t ist;	      // Interrupt Stack Table offset
	uint8_t type_attr;    // Type и атрибуты
	uint16_t offset_mid;  // Средние 16 бит адреса
	uint32_t offset_high; // Старшие 32 бита адреса
	uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

// IDT Pointer для LIDT
typedef struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr_t;

// ============================================================================
// Константы
// ============================================================================

// GDT Access flags
#define GDT_ACCESS_PRESENT (1 << 7)
#define GDT_ACCESS_RING0 (0 << 5)
#define GDT_ACCESS_RING3 (3 << 5)
#define GDT_ACCESS_SYSTEM (1 << 4)
#define GDT_ACCESS_EXECUTABLE (1 << 3)
#define GDT_ACCESS_DC (1 << 2)
#define GDT_ACCESS_RW (1 << 1)
#define GDT_ACCESS_ACCESSED (1 << 0)

// GDT Granularity flags
#define GDT_GRAN_4K (1 << 7)
#define GDT_GRAN_32BIT (1 << 6)
#define GDT_GRAN_64BIT (1 << 5)

// TSS Access byte
#define TSS_ACCESS 0x89 // Present, Ring 0, TSS Available

// IDT Type attributes
#define IDT_TYPE_INTERRUPT 0x8E // Present, Ring 0, 64-bit Interrupt Gate
#define IDT_TYPE_TRAP 0x8F	// Present, Ring 0, 64-bit Trap Gate
#define IDT_TYPE_USER 0xEE	// Present, Ring 3, 64-bit Interrupt Gate

// Селекторы
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA 0x1B // index 3 | RPL=3
#define GDT_USER_CODE 0x23 // index 4 | RPL=3
#define GDT_TSS 0x28

// Количество прерываний
#define IDT_ENTRIES 256

// ============================================================================
// Функции
// ============================================================================

void gdt_init(void);
void tss_init(void);
void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t type_attr);
uint64_t get_gdt_base(void);
uint16_t get_gdt_limit(void);
void idt_load();

// Внешние ассемблерные функции
extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_flush(uint16_t tss_selector);
extern void idt_flush(uint64_t idt_ptr_addr);
