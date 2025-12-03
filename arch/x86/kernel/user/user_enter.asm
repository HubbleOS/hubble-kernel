; user_enter.asm
; Jump from kernel (ring 0) to user mode (ring 3)
; Arguments:
;   rdi - entry point (VA) of user program
;   rsi - user stack top (VA)

[BITS 64]
global user_enter
user_enter:
    cli                    ; disable interrupts

    ; --- Load user segments ---
    mov ax, 0x23           ; user data segment (RPL=3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; --- Setup user stack ---
    mov rsp, rsi           ; top of user stack

    ; --- IRETQ stack frame ---
    push 0x23              ; SS for user mode
    push rsi               ; RSP for user mode
    pushfq                 ; RFLAGS
    or dword [rsp], 0x200  ; enable IF
    push 0x1B              ; CS for user mode (RPL=3)
    push rdi               ; RIP (user entry)

    iretq                  ; jump to user code
    hlt                     ; should never reach

