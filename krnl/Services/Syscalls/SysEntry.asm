[BITS 64]
global SysEntry
extern SysDisp ; System Dispatcher
extern TestingGSBase

section .text

SysEntry:
    swapgs             ; Swap to kernel GS

    mov [gs:24], r10
    mov r10, [gs:8]    ; r10
    mov [r10 + 144], rsp ; usr_stack_save
    mov rsp, [r10 + 128]  ; krnl_stack_top

    push r11
    push rcx
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    push r9
    push r8
    push qword [gs:24]
    push rdx
    push rsi
    push rdi
    push rax

    call SysDisp

    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx
    pop r11

    mov r10, [gs:8]
    mov rsp, [r10 + 144] ; Load usr_stack_save back to RSP

    swapgs
    o64 sysret