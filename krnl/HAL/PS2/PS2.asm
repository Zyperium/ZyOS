[BITS 64]
default abs
global PS2Keyboard
global PS2Mouse
extern KBHI_Wrapper
extern MSHI_Wrapper

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
    jz .done
    swapgs
    or qword [rsp + 32], 3

    cmp qword [rsp + 32], 0x23
    je .done

    mov qword [rsp + 32], 0x23
    mov qword [rsp + 8], 0x1B
.done:

    iretq

PS2Mouse:
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
    call MSHI_Wrapper

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

    test qword [rsp + 8], 3
    jz .done
    swapgs
    or qword [rsp + 32], 3

    cmp qword [rsp + 32], 0x23
    je .done

    mov qword [rsp + 32], 0x23
    mov qword [rsp + 8], 0x1B
.done:
    iretq