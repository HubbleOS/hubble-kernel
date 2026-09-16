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

/* -- CPU Exception Vectors -------------------------------------- */

#define EXC_DIVIDE_BY_ZERO 0
#define EXC_DEBUG 1
#define EXC_NMI 2
#define EXC_BREAKPOINT 3
#define EXC_OVERFLOW 4
#define EXC_BOUND_RANGE_EXCEEDED 5
#define EXC_INVALID_OPCODE 6
#define EXC_DEVICE_NOT_AVAILABLE 7
#define EXC_DOUBLE_FAULT 8
#define EXC_COPROCESSOR_SEGMENT_OVERRUN 9
#define EXC_INVALID_TSS 10
#define EXC_SEGMENT_NOT_PRESENT 11
#define EXC_STACK_SEGMENT_FAULT 12
#define EXC_GENERAL_PROTECTION_FAULT 13
#define EXC_PAGE_FAULT 14
#define EXC_X87_FLOATING_POINT_EXCEPTION 16
#define EXC_ALIGNMENT_CHECK 17
#define EXC_MACHINE_CHECK 18
#define EXC_SIMD_FLOATING_POINT_EXCEPTION 19
#define EXC_VIRTUALIZATION_EXCEPTION 20
#define EXC_CONTROL_PROTECTION_EXCEPTION 21

/**
 * @brief CPU register state snapshot pushed during an interrupt/exception
 *
 * Contains general-purpose registers saved manually in ISR stubs,
 * the interrupt number / error code, and the CPU-pushed frame
 * (RIP, CS, RFLAGS, RSP, SS).
 */
typedef struct registers {
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

/* -- Initialisation -------------------------------------------- */

void interrupts_init(void);

/* -- Legacy PIC ------------------------------------------------ */

void pic_remap(void);
void pic_send_eoi(uint8_t irq);

/* -- IRQ Masking ----------------------------------------------- */

void irq_set_mask(uint8_t irq);
void irq_clear_mask(uint8_t irq);

/* -- Handler Registry ------------------------------------------ */

void irq_install_handler(uint8_t irq, irq_handler_t handler);
void irq_uninstall_handler(uint8_t irq);

/* -- Common Handlers ------------------------------------------- */

void isr_handler(registers_t *regs);
void irq_handler(registers_t *regs);

/* -- Assembly Stubs -------------------------------------------- */

extern void df_entry(void);
