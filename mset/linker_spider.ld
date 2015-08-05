ENTRY(_start)
SECTIONS
{
    . = 0x009D2000;
    .text.start : { *(.text.start) }
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss COMMON) }
    .rodata : { *(.rodata) }
    . = ALIGN(4);
}
