OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(_start)
SECTIONS
{
    . = 0x40000000;

    __start__ = .;

    .text       : ALIGN(4) { *(.text.start) *(.text*); . = ALIGN(4); }
    .rodata     : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data       : ALIGN(4) { *(.data*); . = ALIGN(4); }
    .bss        : ALIGN(8) { __bss_start__ = .; *(.bss* COMMON); . = ALIGN(8); __bss_end__ = .; }

    . = ALIGN(0x1000);

    __end__ = .;
}
