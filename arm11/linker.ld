OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)

ENTRY(_start)
SECTIONS
{
    . = 0x1FF80000;

    .text   : ALIGN(4) { *(.text.start) *(.text*); . = ALIGN(4); }
    .rodata : ALIGN(4) { *(.rodata*); . = ALIGN(4); }
    .data   : ALIGN(4) { *(.data*); . = ALIGN(4); }
    .bss    : ALIGN(8) { __bss_start = .; *(.bss* COMMON); . = ALIGN(8); __bss_end = .; }

    __stack_top__    = 0x1FFFE000;
    . = ALIGN(4);
}
