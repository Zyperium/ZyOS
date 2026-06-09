[BITS 64]
extern exception_handler

global ignore_handler

%macro push_a 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro pop_a 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

ignore_handler:
    iretq

interrupt_common:
    push_a

    mov rdi, rsp

    call exception_handler
    
    pop_a

    add rsp, 16

    iretq

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp interrupt_common
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp interrupt_common
%endmacro

ISR_NOERRCODE 0   ; Division by Zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; NMI
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound Range
ISR_NOERRCODE 6   ; Invalid Opcode
ISR_NOERRCODE 7   ; Device Not Available
ISR_ERRCODE   8   ; Double Fault
ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment Not Present
ISR_ERRCODE   12  ; Stack-Segment Fault
ISR_ERRCODE   13  ; General Protection Fault
ISR_ERRCODE   14  ; Page Fault
ISR_NOERRCODE 16  ; x87 Floating-Point Exception