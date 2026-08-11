
section .text
global _start
extern main
_start:
    ; There isn't a lot to do here (yet)
    sub rsp, 8

    call main

    add rsp, 8
    
    mov rax, 10
    syscall ; terminate

    jmp $