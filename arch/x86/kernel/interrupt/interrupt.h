/**
 * @file interrupt.h
 * @brief Interrupt handling functions and types
 *
 * Declares the register snapshot type, IRQ handler signature,
 * and public API for PIC, IRQ management, and common handler
 * dispatch.
 */

#pragma once

#include <stdint.h>

/**
 * @brief CPU register state snapshot pushed during an interrupt/exception
 *
 * Contains general-purpose registers saved manually in ISR stubs,
 * the interrupt number / error code, and the CPU-pushed frame
 * (RIP, CS, RFLAGS, RSP, SS).
 */
typedef struct registers
{
	uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
	uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

	uint64_t int_no, err_code;

	uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;

/**
 * @brief Type of an IRQ handler function
 *
 * @param regs Pointer to saved CPU register state
 */
typedef void (*irq_handler_t)(registers_t *regs);

/* ── Initialisation ──────────────────────────────────────────── */

void interrupts_init(void);

/* ── Legacy PIC ──────────────────────────────────────────────── */

void pic_remap(void);
void pic_send_eoi(uint8_t irq);

/* ── IRQ Masking ─────────────────────────────────────────────── */

void irq_set_mask(uint8_t irq);
void irq_clear_mask(uint8_t irq);

/* ── Handler Registry ────────────────────────────────────────── */

void irq_install_handler(uint8_t irq, irq_handler_t handler);
void irq_uninstall_handler(uint8_t irq);

/* ── Common Handlers ─────────────────────────────────────────── */

void isr_handler(registers_t *regs);
void irq_handler(registers_t *regs);
