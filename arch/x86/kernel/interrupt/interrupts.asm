; interrupts.asm

; Interrupt Service Routines (ISR) - Assembly Stubs

[BITS 64]

; Exporting handlers
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
global isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21
global irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
global irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
global isr128

; Importing common handlers from C
extern isr_handler
extern irq_handler
extern syscall_handler_wrapper

; Macros for creating ISRs

; Macro for interrupts WITHOUT error code
%macro ISR_NOERRCODE 1
isr%1:
    push qword 0            ; Dummy error code
    push qword %1           ; Номер прерывания
    jmp isr_common_stub
%endmacro

; Macro for interrupts With error code
%macro ISR_ERRCODE 1
isr%1:
    push qword %1           ; Interrupt number (the error code is already on the stack)
    jmp isr_common_stub
%endmacro

; Macro for IRQ
%macro IRQ 2
irq%1:
    push qword 0            ; Dummy error code
    push qword %2           ; Interrupt number (32 + IRQ number)
    jmp irq_common_stub
%endmacro

; CPU Exceptions (0-21)
ISR_NOERRCODE 0     ; Division By Zero
ISR_NOERRCODE 1     ; Debug
ISR_NOERRCODE 2     ; Non Maskable Interrupt
ISR_NOERRCODE 3     ; Breakpoint
ISR_NOERRCODE 4     ; Overflow
ISR_NOERRCODE 5     ; Bound Range Exceeded
ISR_NOERRCODE 6     ; Invalid Opcode
ISR_NOERRCODE 7     ; Device Not Available
ISR_ERRCODE   8     ; Double Fault (with error code)
ISR_NOERRCODE 9     ; Coprocessor Segment Overrun
ISR_ERRCODE   10    ; Invalid TSS (with error code)
ISR_ERRCODE   11    ; Segment Not Present (with error code)
ISR_ERRCODE   12    ; Stack-Segment Fault (with error code)
ISR_ERRCODE   13    ; General Protection Fault (with error code)
ISR_ERRCODE   14    ; Page Fault (with error code)
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; x87 Floating-Point Exception
ISR_ERRCODE   17    ; Alignment Check (with error code)
ISR_NOERRCODE 18    ; Machine Check
ISR_NOERRCODE 19    ; SIMD Floating-Point Exception
ISR_NOERRCODE 20    ; Virtualization Exception
ISR_ERRCODE   21    ; Control Protection Exception (with error code)

; Hardware Interrupts (IRQ 0-15 -> INT 32-47)
IRQ 0,  32          ; Timer
IRQ 1,  33          ; Keyboard
IRQ 2,  34          ; Cascade
IRQ 3,  35          ; COM2
IRQ 4,  36          ; COM1
IRQ 5,  37          ; LPT2
IRQ 6,  38          ; Floppy
IRQ 7,  39          ; LPT1
IRQ 8,  40          ; RTC
IRQ 9,  41          ; Free
IRQ 10, 42          ; Free
IRQ 11, 43          ; Free
IRQ 12, 44          ; PS/2 Mouse
IRQ 13, 45          ; FPU
IRQ 14, 46          ; Primary ATA
IRQ 15, 47          ; Secondary ATA

; System Call (INT 0x80 = 128)
isr128:
    push qword 0            ; Dummy error code
    push qword 128          ; Interrupt number
    jmp syscall_common_stub

; Common handler for ISR (CPU Exceptions)
isr_common_stub:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; We install the correct kernel segments (WITHOUT saving)
    mov ax, 0x10        ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; We align the stack by 16 bytes for the ABI
    mov rbp, rsp        ; We keep the original RSP
    and rsp, ~0xF       ; Align the stack
    
    ; We call the C handler
    mov rdi, rbp        ; We pass the pointer to registers_t
    call isr_handler
    
    ; Restore the original stack
    mov rsp, rbp
    
    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Clearing the stack from int_no and err_code
    add rsp, 16
    
    ; Returning from interruption
    iretq

; General handler for IRQ (Hardware Interrupts)
irq_common_stub:
    ; Save all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Install the correct kernel segments (WITHOUT saving)
    mov ax, 0x10        ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Align the stack to 16 bytes for ABI
    mov rbp, rsp        ; We keep the original RSP
    and rsp, ~0xF       ; Aligning the stack
    
    ; Calling the C handler
    mov rdi, rbp        ; Passing a pointer to registers_t
    call irq_handler
    
    ; Restoring the original stack
    mov rsp, rbp
    
    ; Restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Clearing the stack from int_no and err_code
    add rsp, 16
    
    ; Returning from interruption
    iretq

; System Calls Handler
syscall_common_stub:
    ; We keep all registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Installing the correct kernel segments
    mov ax, 0x10        ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    
    ; Align the stack to 16 bytes for ABI
    mov rbp, rsp        ; We keep the original RSP
    and rsp, ~0xF       ; Aligning the stack
    
    ; Calling the C wrapper
    mov rdi, rbp        ; Passing a pointer to registers_t
    call syscall_handler_wrapper
    
    ; Restoring the original stack
    mov rsp, rbp
    
    ; We restore all registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax ; res
    
    ; Clearing the stack from int_no and err_code
    add rsp, 16
    
    ; Returning from interruption
    iretq
