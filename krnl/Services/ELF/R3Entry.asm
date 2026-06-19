[BITS 64]
global R3Entry


section .text
; void R3Entry(uint64_t entry_point)
; RDI = entry_point
; RSI = FS Base
; RDX = Ring 3 RSP

; Jumps from ring 0 to ring 3. DO NOT call this on a task that 
; needs to be in ring 0 (duh)
R3Entry:
    cli

    swapgs

    mov r9, rdx ; Save Ring 3 RSP
    mov r8, rsi ; Save FS base

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ecx, 0xC0000100 ; FS Base address
    mov rax, r8
    mov rdx, r8
    shr rdx, 32
    wrmsr

    ; Ring 3 IRETQ stack
    push 0x23 ; R3 RPL
    push r9  ; RSP
    push 0x202 ; Clean RFLAGS
    push 0x1B ; R3 Code Segment
    push rdi ; Entry point!

    ; Clean general registers to prevent data leakage.
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    xor r8d, r8d
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor r15d, r15d

    iretq