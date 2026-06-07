ENTRY(krnlmain)

__PHYSICAL_BASE = 0x100000;
__VIRTUAL_BASE = 0xffffffff80000000;

PHDRS
{
    text    PT_LOAD    FLAGS(5); /* (1<<0)|(1<<2) -> Execute (1) + Read (4) = 5 */
    rodata  PT_LOAD    FLAGS(4); /* (1<<2)        -> Read (4) */
    data    PT_LOAD    FLAGS(6); /* (1<<1)|(1<<2) -> Write (2) + Read (4) = 6 */
}

SECTIONS
{
    . = __VIRTUAL_BASE + __PHYSICAL_BASE;
    .text : AT(ADDR(.text) - __VIRTUAL_BASE) {
        *(.text .text.*)

    } :text

    . = ALIGN(0x1000);
    .rodata : AT(ADDR(.rodata) - __VIRTUAL_BASE) {
        *(.rodata .rodata.*)

    } :rodata

    . = ALIGN(0x1000);
    .data : AT(ADDR(.data) - __VIRTUAL_BASE) {
        *(.data .data.*)

    } :data

    .bss : AT(ADDR(.bss) - __VIRTUAL_BASE) {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    _kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }

    .init_array : {
        _init_array_start = .;
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        _init_array_end = .;
    }
}