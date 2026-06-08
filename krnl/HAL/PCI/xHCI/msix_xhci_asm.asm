global xHCIHandler
extern xHCIIntHandler

xHCIHandler:
    cld

    mov rax, [rsp + 128] ; (15 * 8 + RIP = 128)
    and rax, 0x03
    cmp rax, 3
    jne .krnl_enter

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
    call xHCIIntHandler

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

    test byte [rsp + 8], 3
    jz .done
    or qword [rsp + 32], 3

.done:
    iretq