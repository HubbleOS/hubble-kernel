; user_enter.asm
; Jump from kernel (ring 0) to user mode (ring 3)
; Arguments:
;   rdi - entry point (VA) of user program
;   rsi - user stack top (VA)

global user_enter

user_enter:
    cli
    
    ; Ensure stack is 16-byte aligned before building IRETQ frame
    and rsp, ~0xF               ; Align RSP to 16 bytes
    
    ; Build IRETQ frame
    push qword 0x23             ; SS
    push rsi                    ; RSP
    
    pushfq
    pop rax
    or rax, 0x202               ; IF + reserved bit 1
    push rax                    ; RFLAGS
    
    push qword 0x1B             ; CS  
    push rdi                    ; RIP
    
    ; Now zero registers
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
    
    iretq
    
    ud2