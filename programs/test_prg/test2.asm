section .data
    sys1_msg db "I am original", 0
    sys1_msg_len equ $ - sys1_msg
    sys2_msg db "I am a clone", 0
    sys2_msg_len equ $ - sys2_msg

section .text
global fork_test

fork_test:
    mov rax, 11 ; fork 
    syscall

    mov rbp, rax

    mov rax, 19 ; get pid
    syscall

    cmp rax, rbp
    je .orig

    mov rax, 5
    lea rdi, [sys2_msg]
    mov rsi, sys2_msg_len
    syscall

    mov rax, 9
    syscall

.orig:
    mov rax, 5
    lea rdi, [sys1_msg]
    mov rsi, sys1_msg_len
    syscall

    mov rax, 9
    syscall
