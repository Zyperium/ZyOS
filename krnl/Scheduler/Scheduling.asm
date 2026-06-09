[BITS 64]

global SchedulerHandler
global QuietSwitch
extern SchedulerSwitch ; C++ function
extern AckInterrupt ; C++ function

section .text
SchedulerHandler:
    cld

    ; check kernel state
    cmp qword [rsp + 8], 0x08
    je .krnl_enter

    swapgs

.krnl_enter:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call SchedulerSwitch

    push rax
    call AckInterrupt
    pop rax

    mov rsp, rax

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    test qword [rsp + 8], 3
    jz .krnl_exit

    swapgs

    or qword [rsp + 32], 3

.krnl_exit:
    iretq

QuietSwitch:
    ret
    cld

.krnl_enter:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    mov rdi, rsp
    call SchedulerSwitch
    mov rsp, rax

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    test qword [rsp + 8], 3
    jz .krnl_exit

    swapgs

    or qword [rsp + 32], 3

.krnl_exit:
    iretq