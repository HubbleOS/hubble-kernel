; usermode.asm - Enter user mode (ring 3)
; NASM syntax

[BITS 64]

global enter_usermode

section .text

enter_usermode:
    ; RDI points to usermode_context_t struct:
    ; struct { uint64_t rip; uint64_t rsp; uint64_t rflags; }
    ; Offset 0 = rip, 8 = rsp, 16 = rflags
    
    ; Save context pointer
    mov r15, rdi
    
    ; Load user data segment (0x23 = GDT entry 4, RPL=3)
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Build iretq stack frame (push in reverse order)
    ; iretq expects: SS, RSP, RFLAGS, CS, RIP (top to bottom)
    
    push 0x23                   ; SS (user stack segment)
    push qword [r15 + 8]        ; RSP (user stack pointer)
    push qword [r15 + 16]       ; RFLAGS
    push 0x1B                   ; CS (user code segment, GDT entry 3, RPL=3)
    push qword [r15 + 0]        ; RIP (entry point)
    
    ; Clear all general-purpose registers for security
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    ; Jump to user mode
    iretq
    
    ; Should never reach here
    ud2
