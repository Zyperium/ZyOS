global tester_func

section .data
    sys_msg db "/A/HELLOW~1.TXT", 0
    msg_len equ $ - sys_msg
    ioctl_ptr db "R0UI/"
    ioctl_len equ $ - ioctl_ptr

section .text
tester_func:
    sub rsp, 80
    mov eax, 0
    lea rdi, [rel sys_msg]
    mov rsi, msg_len
    syscall

    ; Now we should have the text file, lets read it!
    mov rdi, rax
    mov eax, 1
    lea r10, [rsp]
    mov rsi, 0
    mov rdx, 64
    ; NOTE: This is a temporary test
    syscall
    mov r9, rax

    mov rax, 4 ; Now we can open an IOCTL with the R0UI
    lea rdi, [rel ioctl_ptr]
    mov rsi, 1 ; Open window
    mov rdx, ioctl_len
    syscall

    mov rax, 5 ; log some stuff
    lea rdi, [rsp]
    mov rsi, r9
    syscall

    mov rax, 9
    syscall

    add rsp, 80
    ret