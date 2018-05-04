OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(_start)
SECTIONS
{
    . = 0x01FF7FE0;

    .text   : ALIGN(4) { *(.text.start) *(.text*); . = ALIGN(4); }
    .rodata : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data   : ALIGN(4) { *(.data*); . = ALIGN(8); *(.bss* COMMON); . = ALIGN(8); }

    . = ALIGN(4);
}
