global tester_func

section .text
tester_func:
    mov rax, 1

    syscall
    ret