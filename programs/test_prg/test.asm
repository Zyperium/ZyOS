section .data
    hlw_text db "Hello, world!", 0
    hlw_len equ $ - hlw_text

    orig_text db "I am original", 0
    orig_len equ $ - orig_text

    fork_text db "I am forkd", 0
    fork_len equ $ - fork_text

section .text
global asm_main
global asm_fork

asm_main:
    sub rsp, 8

    mov rax, 5
    lea rdi, [hlw_text]
    mov rsi, hlw_len
    syscall

    mov rax, 19
    syscall

    push rax
    mov rax, 11
    syscall

    pop rbx

    cmp rax, rbx
    je .original

    mov rax, 5
    lea rdi, [fork_text]
    mov rsi, fork_len
    syscall

    mov rax, 9
    syscall

    add rsp, 8
    jmp $

    ret

.original:
    mov rax, 5
    lea rdi, [orig_text]
    mov rsi, orig_len
    syscall

    mov rax, 9
    syscall

    add rsp, 8
    jmp $

    ret

asm_fork:
    sub rsp, 8
    mov rax, 11
    syscall
    push rax

    mov rax, 5
    lea rdi, [orig_text]
    mov rsi, orig_len

    pop rax
    add rsp, 8

    ret