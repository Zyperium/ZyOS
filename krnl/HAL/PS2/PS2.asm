[BITS 64]
default abs
%define lapic_base_ptr _ZN3HAL4CORE14lapic_base_ptrE
global PS2Keyboard
extern KBHI_Wrapper
extern lapic_base_ptr

PS2Keyboard:
    cld
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

    mov rax, [rsp + 128] 
    and rax, 0x03
    cmp rax, 3
    jne .in_kernel

    swapgs

.in_kernel:
    mov rdi, rsp 
    call KBHI_Wrapper

    mov rax, [rsp + 128]
    and rax, 0x03
    cmp rax, 3
    jne .exit_kernel
    swapgs

.exit_kernel:
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

    push rax
    push rcx
    mov rcx, [lapic_base_ptr]
    mov dword [rcx + 0xB0], 0
    pop rcx
    pop rax

    test byte [rsp + 8], 3
    jz .done
    or qword [rsp + 32], 3
.done:

    iretq
