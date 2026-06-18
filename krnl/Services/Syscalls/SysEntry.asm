[BITS 64]
global SysEntry
extern SysDisp ; System Dispatcher

section .text

SysEntry:
    swapgs ; swap to kernel GS, gives us access to all the goodies!
    ; For the GS, view ThreadLocal to check offsets & alignment
    mov [gs:24], r10
    mov r10, [gs:8]
    mov [r10 + 112], rsp ; usr_stack_save. If you change the Task class, CHANGE THIS TOO!
    mov rsp, [r10 + 96] ; krnl_stack_top

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
    push qword [gs:24] ; R10
    push rdx
    push rsi
    push rdi
    push rax

    pop rdi
    pop rsi
    pop rdx
    pop rdx
    pop r8
    pop r9
    add rsp, 8
    
    call SysEntry ; Result in RAX

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop rcx
    pop r11

    mov rsp, [gs:24]

    swapgs

    o64 sysret