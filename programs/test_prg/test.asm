global tester_func

section .data
    sys_msg db "/A/HELLOW~1.TXT", 0
    msg_len equ $ - sys_msg

section .text
tester_func:
    mov eax, 0
    lea rdi, [rel sys_msg]
    mov rsi, msg_len
    syscall

    ; Now we should have the text file, lets read it!
    mov rdi, rax
    mov eax, 1
    ; NOTE: This is a temporary test
    syscall
    ret