/**
 * @file gdt.h
 * @brief Global Descriptor Table, TSS, and IDT type definitions
 *
 * Defines the x86-64 GDT entry layout, TSS descriptor and
 * structure, IDT entry layout, related constants, and
 * public function prototypes for segmentation and interrupt
 * table setup.
 */

#pragma once

#include <stdint.h>

/* ── GDT Entry ------------------------------------------------- */

/**
 * @brief 8-byte GDT descriptor entry
 */
typedef struct
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

/* ── TSS Descriptor (16 bytes in 64-bit mode) ────────────────── */

/**
 * @brief 16-byte TSS system segment descriptor
 */
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

/* ── GDT Pointer (for LGDT) ──────────────────────────────────── */

/**
 * @brief Pointer/length structure passed to LGDT
 */
typedef struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

/* ── TSS (Task State Segment) ────────────────────────────────── */

/**
 * @brief Software TSS structure for privilege-level stack switching
 */
typedef struct
{
	uint32_t reserved0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved1;
	uint64_t ist[7];
	uint64_t reserved2;
	uint16_t reserved3;
	uint16_t iomap_base;
} __attribute__((packed)) tss_t;

/* ── IDT Entry ------------------------------------------------- */

/**
 * @brief 16-byte IDT gate descriptor
 */
typedef struct
{
	uint16_t offset_low;
	uint16_t selector;
	uint8_t ist;
	uint8_t type_attr;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t reserved;
} __attribute__((packed)) idt_entry_t;

/* ── IDT Pointer (for LIDT) ──────────────────────────────────── */

/**
 * @brief Pointer/length structure passed to LIDT
 */
typedef struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr_t;

/* ── GDT Access Flags ────────────────────────────────────────── */

#define GDT_ACCESS_PRESENT (1 << 7)
#define GDT_ACCESS_RING0 (0 << 5)
#define GDT_ACCESS_RING3 (3 << 5)
#define GDT_ACCESS_SYSTEM (1 << 4)
#define GDT_ACCESS_EXECUTABLE (1 << 3)
#define GDT_ACCESS_DC (1 << 2)
#define GDT_ACCESS_RW (1 << 1)
#define GDT_ACCESS_ACCESSED (1 << 0)

/* ── GDT Granularity Flags ───────────────────────────────────── */

#define GDT_GRAN_4K (1 << 7)
#define GDT_GRAN_32BIT (1 << 6)
#define GDT_GRAN_64BIT (1 << 5)

/* ── TSS Access Byte ─────────────────────────────────────────── */

#define TSS_ACCESS 0x89

/* ── IDT Type Attributes ─────────────────────────────────────── */

#define IDT_TYPE_INTERRUPT 0x8E
#define IDT_TYPE_TRAP 0x8F
#define IDT_TYPE_USER 0xEE

/* ── Selectors ───────────────────────────────────────────────── */

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA 0x1B
#define GDT_USER_CODE 0x23
#define GDT_TSS 0x28

/* ── Constants ───────────────────────────────────────────────── */

#define IDT_ENTRIES 256

/* ── Public Functions ────────────────────────────────────────── */

void gdt_init(void);
void tss_init(void);
void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t type_attr);
uint64_t get_gdt_base(void);
uint16_t get_gdt_limit(void);
void idt_load(void);
void tss_set_rsp0(uint64_t rsp0);

/* ── Assembly Routines ───────────────────────────────────────── */

extern void gdt_flush(uint64_t gdt_ptr_addr);
extern void tss_flush(uint16_t tss_selector);
extern void idt_flush(uint64_t idt_ptr_addr);
