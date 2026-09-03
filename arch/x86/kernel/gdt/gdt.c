/**
 * @file gdt.c
 * @brief Global Descriptor Table, TSS, and IDT initialisation
 *
 * Sets up the x86-64 GDT with kernel/user code/data segments,
 * per-CPU TSS entries for IST and ring-0 stack switching, and
 * the full IDT with CPU exception and IRQ gate entries.
 */

#include "gdt.h"
#include <hubble/printk.h>
#include <hubble/string.h>

#include <apic/apic.h>

/* -- Constants ------------------------------------------------- */

#define MAX_CPUS 8

/* --- Global Tables -------------------------------------------- */

static struct {
  gdt_entry_t entries[5];
  tss_entry_t tss_descriptor;
} __attribute__((packed, aligned(16))) gdt_table;

static gdt_ptr_t gdt_ptr;

static tss_t tss[MAX_CPUS];
static uint8_t kernel_stacks[MAX_CPUS][16384] __attribute__((aligned(16)));

/* -- GDT Helpers (static) -------------------------------------- */

/**
 * @brief Program one GDT entry
 *
 * @param num    Index in the GDT
 * @param base   Segment base address
 * @param limit  Segment limit
 * @param access Access byte
 * @param gran   Granularity byte
 */
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access,
                         uint8_t gran) {
  gdt_entry_t *entry = &gdt_table.entries[num];

  entry->base_low = (base & 0xFFFF);
  entry->base_middle = (base >> 16) & 0xFF;
  entry->base_high = (base >> 24) & 0xFF;

  entry->limit_low = (limit & 0xFFFF);
  entry->granularity = (limit >> 16) & 0x0F;
  entry->granularity |= gran & 0xF0;

  entry->access = access;
}

/**
 * @brief Program the TSS descriptor in the GDT
 *
 * @param base  Physical address of the TSS
 * @param limit Size of the TSS minus one
 */
static void tss_set_descriptor(uint64_t base, uint32_t limit) {
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

/* -- GDT Initialisation ---------------------------------------- */

/**
 * @brief Initialise the GDT with standard x86-64 segments
 *
 * Sets up NULL, kernel code/data, user code/data, and a
 * placeholder TSS descriptor.  Loads the table via LGDT.
 */
void gdt_init(void) {
  gdt_ptr.limit = sizeof(gdt_table) - 1;
  gdt_ptr.base = (uint64_t)&gdt_table;

  printk(KERN_INFO "GDT Pointer: %p\n", gdt_ptr.base);

  memset(&gdt_table, 0, sizeof(gdt_table));

  gdt_set_gate(0, 0, 0, 0, 0);

  gdt_set_gate(1, 0, 0xFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM |
                   GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_64BIT);

  gdt_set_gate(2, 0, 0xFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_SYSTEM |
                   GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_64BIT);

  gdt_set_gate(3, 0, 0xFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM |
                   GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_64BIT);

  gdt_set_gate(4, 0, 0xFFFFF,
               GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_SYSTEM |
                   GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
               GDT_GRAN_4K | GDT_GRAN_64BIT);

  tss_set_descriptor((uint64_t)&tss, sizeof(tss) - 1);

  gdt_flush((uint64_t)&gdt_ptr);
}

/**
 * @brief Get the base address of the GDT
 *
 * @return Virtual address of the GDT
 */
uint64_t get_gdt_base(void) {
  printk(KERN_INFO "GDT Base: %p\n", gdt_ptr.base);
  return gdt_ptr.base;
}

/**
 * @brief Get the GDT limit
 *
 * @return Limit value
 */
uint16_t get_gdt_limit(void) { return gdt_ptr.limit; }

/* -- TSS Management -------------------------------------------- */

/**
 * @brief Set the Ring-0 stack pointer for the current CPU
 *
 * @param rsp0 Stack pointer value
 */
void tss_set_rsp0(uint64_t rsp0) {
  uint8_t cpu_id = lapic_get_id();
  tss[cpu_id].rsp0 = rsp0;
}

/**
 * @brief Get the APIC ID via CPUID (works before APIC is initialized)
 *
 * Uses CPUID leaf 1, EBX bits 24-31 to read the initial APIC ID.
 * This is safe to call during early boot when the LAPIC MMIO is
 * not yet mapped.
 */
static uint8_t early_get_apic_id(void) {
  uint32_t eax, ebx, ecx, edx;
  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
  return (ebx >> 24) & 0xFF;
}

/**
 * @brief Initialise the TSS for the current CPU
 *
 * Allocates a kernel stack and loads the TSS via LTR.
 * Uses CPUID for the APIC ID since the LAPIC MMIO is not
 * yet available during early boot.
 */
void tss_init(void) {
  uint8_t cpu_id = early_get_apic_id();

  memset(&tss[cpu_id], 0, sizeof(tss_t));
  tss[cpu_id].rsp0 =
      (uint64_t)(kernel_stacks[cpu_id] + sizeof(kernel_stacks[cpu_id]));

  tss_set_descriptor((uint64_t)&tss[cpu_id], sizeof(tss_t) - 1);

  tss_flush(GDT_TSS);
}

/* -- IDT ------------------------------------------------------- */

static idt_entry_t idt_entries[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

extern void isr128(void);

/* -- IDT Helpers ----------------------------------------------- */

/**
 * @brief Program one IDT entry
 *
 * @param num      Vector index (0-255)
 * @param handler  Handler function address
 * @param selector Code segment selector
 * @param type_attr Gate type / DPL / present bits
 */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector,
                  uint8_t type_attr) {
  idt_entries[num].offset_low = handler & 0xFFFF;
  idt_entries[num].offset_mid = (handler >> 16) & 0xFFFF;
  idt_entries[num].offset_high = (handler >> 32) & 0xFFFFFFFF;

  idt_entries[num].selector = selector;
  idt_entries[num].ist = 0;
  idt_entries[num].type_attr = type_attr;
  idt_entries[num].reserved = 0;
}

/* -- IDT Initialisation ---------------------------------------- */

/**
 * @brief Initialise the IDT with all CPU exception and IRQ gates
 *
 * Maps exceptions 0-21, IRQs 32-47, and syscall vector 128.
 * Loads the table via LIDT.
 */
void idt_init(void) {
  idt_ptr.limit = sizeof(idt_entries) - 1;
  idt_ptr.base = (uint64_t)&idt_entries;

  memset(&idt_entries, 0, sizeof(idt_entries));

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

  idt_set_gate(128, (uint64_t)isr128, GDT_KERNEL_CODE, IDT_TYPE_USER);

  idt_flush((uint64_t)&idt_ptr);
}

/**
 * @brief Reload the IDT (used after suspend/resume or AP init)
 */
void idt_load(void) { idt_flush((uint64_t)&idt_ptr); }
