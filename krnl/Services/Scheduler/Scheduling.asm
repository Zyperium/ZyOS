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

    test qword [rsp + 8], 3 ; Pretty sure this is the SS
    jz .krnl_exit

    swapgs

    or qword [rsp + 32], 3 ; OR code segment

.krnl_exit:
    iretq

QuietSwitch:
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
    
    ; For some reason this needs to happen on a manual Yield(); 
    ; Otherwise the CS & SS can be swapped for ring 3 tasks. Why? IDK.
    ; If you can isolate a root cause, please let me know.

    cmp qword [rsp + 32], 0x23 ; Code segment should be 0x23
    je .krnl_exit

    ; Code Segment and Stack Segment are likely swapped!
    mov qword [rsp + 8], 0x1B
    mov qword [rsp + 32], 0x23 ; Hopefully this fixes it immediately [so future calls can skip straight to .krnl_exit]

.krnl_exit:
    iretq