; -- Context Switch --------------------------------------------
; Assembly routines for task context switching.
; switch_to_task loads a new task context and performs IRETQ
; to transfer control to the new task.
; --------------------------------------------------------------

[BITS 64]

global switch_to_task
global switch_to_kernel_task
global save_context

extern lapic_eoi

; void switch_to_task(cpu_context_t *old, cpu_context_t *new)
; RDI = old context (can be NULL)
; RSI = new context
switch_to_task:

.skip_save:
    ; Load new context (RSI = new context)

    ; Restore FPU/SSE state
    mov     rax, [rsi + 200]
    test    rax, rax
    jz      .skip_fpu_load
    fxrstor [rax]

.skip_fpu_load:
    ; Restore segment selectors
    mov     ax, [rsi + 168]     ; ds
    mov     ds, ax
    mov     ax, [rsi + 176]     ; es
    mov     es, ax
    mov     ax, [rsi + 184]     ; fs
    mov     fs, ax
    mov     ax, [rsi + 192]     ; gs
    mov     gs, ax

    push    rsi                 ; save before call
    call    lapic_eoi
    pop     rsi                 ; restore after call
    sti

    ; Restore general purpose registers
    mov     r15, [rsi + 0]
    mov     r14, [rsi + 8]
    mov     r13, [rsi + 16]
    mov     r12, [rsi + 24]
    mov     r11, [rsi + 32]
    mov     r10, [rsi + 40]
    mov     r9,  [rsi + 48]
    mov     r8,  [rsi + 56]
    ; Skip RDI and RSI for now
    mov     rbp, [rsi + 80]
    mov     rbx, [rsi + 96]
    mov     rdx, [rsi + 104]
    mov     rcx, [rsi + 112]
    mov     rax, [rsi + 120]

    push    qword [rsi + 160]   ; SS
    push    qword [rsi + 128]   ; RSP
    push    qword [rsi + 144]   ; RFLAGS
    push    qword [rsi + 152]   ; CS
    push    qword [rsi + 136]   ; RIP
    mov     rdi, [rsi + 64]     ; restore rdi
    mov     rsi, [rsi + 72]     ; restore rsi LAST (kills context pointer)
    iretq

; void switch_to_kernel_task(cpu_context_t *new) -- never returns.
;
; iretq only reloads RSP/SS from the stack when the CS it pops has a
; *different* RPL than the CPL it's returning from - a "same privilege
; level" return leaves RSP exactly where it was (just past the popped
; RIP/CS/RFLAGS) instead of switching to the target's own stack. Every
; interrupt/exception handler in this kernel runs at CPL0 regardless of
; what was interrupted, so an ordinary schedule() -> task_state_load() ->
; (shared COMMON_STUB epilogue) -> iretq switch works correctly for a
; *user* target (CS RPL3, an actual privilege change, so RSP/SS really do
; get reloaded from the values task_state_load wrote into the frame) but
; silently does NOT switch stacks for a *kernel* target (idle_task,
; kmain_thread - CS RPL0, same as the handler's own CPL): the target
; would start running on top of whatever kernel stack the interrupted
; task happened to be using, not its own task->exec.rsp0-backed one.
; With two schedulable kernel tasks (idle + kmain_thread) unavoidably
; sharing every runqueue alongside user tasks, that happens on basically
; every idle<->user transition - exactly the kind of intermittent,
; SMP/timing-dependent stack corruption that later surfaces as an
; unrelated-looking #GP.
;
; Used instead of the iretq path whenever the target's saved context.cs
; is ring0: explicitly loads the target's own rsp before restoring
; anything else, then reaches its rip with a plain ret instead of iretq
; (no privilege change is happening, so none of iretq's frame is needed).
switch_to_kernel_task:
    mov     rsi, rdi

    mov     rax, [rsi + 200]
    test    rax, rax
    jz      .skip_fpu_load
    fxrstor [rax]
.skip_fpu_load:

    mov     ax, [rsi + 168]     ; ds
    mov     ds, ax
    mov     ax, [rsi + 176]     ; es
    mov     es, ax
    mov     ax, [rsi + 184]     ; fs
    mov     fs, ax
    mov     ax, [rsi + 192]     ; gs
    mov     gs, ax

    push    rsi
    call    lapic_eoi
    pop     rsi
    sti

    ; Switch to the target's own kernel stack now, before restoring any
    ; GP registers - and stash its RIP as the return address for the
    ; final `ret` below, on that same (correct) stack.
    mov     rsp, [rsi + 128]    ; new task's saved rsp
    push    qword [rsi + 136]   ; rip

    mov     r15, [rsi + 0]
    mov     r14, [rsi + 8]
    mov     r13, [rsi + 16]
    mov     r12, [rsi + 24]
    mov     r11, [rsi + 32]
    mov     r10, [rsi + 40]
    mov     r9,  [rsi + 48]
    mov     r8,  [rsi + 56]
    mov     rbp, [rsi + 80]
    mov     rbx, [rsi + 96]
    mov     rdx, [rsi + 104]
    mov     rcx, [rsi + 112]
    mov     rax, [rsi + 120]
    mov     rdi, [rsi + 64]     ; restore rdi
    mov     rsi, [rsi + 72]     ; restore rsi LAST (kills context pointer)

    ret
