global LoadGDT

LoadGDT:
    lgdt [rdi]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    push 0x08
    lea rax, [rel .reload]
    push rax
    o64 retf

.reload:
    ret