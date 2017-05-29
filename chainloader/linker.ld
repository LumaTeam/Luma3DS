OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(_start)
SECTIONS
{
    . = 0x01FF9000;
    
    __start__ = ABSOLUTE(.);

    .text   : ALIGN(4) { *(.text.start) *(.text*); . = ALIGN(4); }
    .rodata : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data   : ALIGN(4) { *(.data*); . = ALIGN(8); *(.bss* COMMON); . = ALIGN(8); }

    . = ALIGN(4);

    __end__ = ABSOLUTE(.);

    __stack_top__    = 0x01FFB800;
    __stack_bottom__ = 0x01FFA800;
}
