; user_enter.asm
; Jump from kernel (ring 0) to user mode (ring 3)
; Arguments:
;   rdi - entry point (VA) of user program
;   rsi - user stack top (VA)

[BITS 64]
global user_enter

user_enter:
    cli                         ; disable interrupts during setup

    ; CRITICAL: DO NOT load user segments here!
    ; Loading DS/ES/FS/GS/SS with RPL=3 selectors in ring 0 causes GPF.
    ; Let IRETQ do all the segment loading.

    ; Build IRETQ stack frame
    ; Stack layout (from bottom to top):
    ; [SS] [RSP] [RFLAGS] [CS] [RIP]
    
    ; Push SS (user data segment with RPL=3)
    push qword 0x23             ; SS = 0x20 | 3
    
    ; Push user stack pointer
    push rsi                    ; RSP
    
    ; Setup RFLAGS with interrupts enabled
    pushfq                      ; Get current RFLAGS
    pop rax
    or rax, 0x200               ; Set IF (bit 9)
    push rax                    ; RFLAGS
    
    ; Push CS (user code segment with RPL=3)
    push qword 0x1B             ; CS = 0x18 | 3
    
    ; Push entry point
    push rdi                    ; RIP
    
    ; Zero out all general-purpose registers for security
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

    ; Execute IRETQ to switch to userspace
    ; This will:
    ; 1. Pop RIP, CS, RFLAGS, RSP, SS from stack
    ; 2. Check CS.RPL (should be 3)
    ; 3. Switch privilege level to ring 3
    ; 4. Load new SS and RSP
    ; 5. Load segment registers with user selectors
    ; 6. Jump to user code
    iretq

    ; Should never reach here
    hlt