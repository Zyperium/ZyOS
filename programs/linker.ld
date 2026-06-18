OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x40000000 + SIZEOF_HEADERS;
    .text : ALIGN(4096) {
        *(.text)
    }

    .rodata : ALIGN(4096) {
        *(.rodata)
    }

    .data : ALIGN(4096) {
        *(.data)
    }

    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss)
    }
    
    PROVIDE(_end = .);    
}