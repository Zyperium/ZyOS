global xHCIHandler
extern xHCIIntHandler

xHCIHandler:
    cld

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
    call xHCIIntHandler

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
    swapgs
    or qword [rsp + 32], 3

    cmp qword [rsp + 32], 0x23 ; Code segment should be 0x23
    je .done

    ; Code Segment and Stack Segment are likely swapped!
    mov qword [rsp + 8], 0x1B
    mov qword [rsp + 32], 0x23

.done:
    iretq