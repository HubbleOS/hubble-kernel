; -- C Runtime Startup ------------------------------------------
; Entry point for the EFI application. Calls efi_main and halts.
; --------------------------------------------------------------

section .text
global _start
extern efi_main

_start:
    sub     rsp, 40
    call    efi_main
    add     rsp, 40

.hang:
    hlt
    jmp     .hang
