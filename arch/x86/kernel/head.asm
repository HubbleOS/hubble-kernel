section .text.boot

extern start_kernel
extern g_boot_info

extern _bss_start
extern _bss_end

global kernel_entry
kernel_entry:
    ; push rdi                        ; save BootInfo*

    ; clear BSS
    ; lea rdi, [rel _bss_start]
    ; lea rcx, [rel _bss_end]
    ; sub rcx, rdi                    ; size in bytes
    ; xor eax, eax
    ; rep stosb                       ; zero out

    ; pop rdi                         ; restore BootInfo*
    mov qword [rel g_boot_info], rdi
    call start_kernel

.hang:
    cli
    hlt
    jmp .hang
