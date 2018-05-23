OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

/* Mostly copied from https://github.com/devkitPro/buildscripts/blob/master/dkarm-eabi/crtls/3dsx.ld */
MEMORY
{
    NULL                : ORIGIN = 0x00000000, LENGTH = 0x1000
    main                : ORIGIN = 0x08006000, LENGTH = 0x080F0000 - 0x08006000
    itcm                : ORIGIN = 0x01FF8000, LENGTH = 0x01FFB800 - 0x01FF8000 /* Unused ITCM slice. */
}

SECTIONS
{
    PROVIDE(__start__           = 0x08006000);
    PROVIDE(__stack_top__       = 0x08100000);
    PROVIDE(__stack_bottom__    = 0x080F0000);

    PROVIDE(__itcm_stack_top__    = 0x01FFB800);
    PROVIDE(__itcm_stack_bottom__ = 0x01FFA800);

    . = __start__;
    . = ALIGN(32);

    .crt0 :
    {
        . = ALIGN(32);
        KEEP( *(.text.start) )
        KEEP( *(.init) )
        . = ALIGN(4);
    } >main

    .itcm_loadable :
    {
        . = ALIGN(32);
        PROVIDE (__itcm_start__  = ABSOLUTE(.));
        PROVIDE (__itcm_lma__    = LOADADDR(.itcm_loadable));

        KEEP(*(.arm9_exception_handlers.text))
        *(.arm9_exception_handlers.text*)
        KEEP(*(.chainloader.text.start))

        chainloader.o(.text*)
        i2c.o(.text*)
        arm9_exception_handlers.o(.text*)

        *(.arm9_exception_handlers.rodata*)
        chainloader.o(.rodata*)
        i2c.o(.rodata*)
        arm9_exception_handlers.o(.rodata*)

        *(.arm9_exception_handlers.data*)
        chainloader.o(.data*)
        i2c.o(.data*)
        arm9_exception_handlers.o(.data*)

        . = ALIGN(8);
    } >itcm AT>main

    .itcm_bss :
    {
        . = ALIGN(8);
        PROVIDE (__itcm_bss_start__ = ABSOLUTE(.));
        *(.arm9_exception_handlers.bss*)
        chainloader.o(.bss* COMMON)
        i2c.o(.bss* COMMON)
        arm9_exception_handlers.o(.bss* COMMON)
        . = ALIGN(8);
        PROVIDE (__itcm_end__ = ABSOLUTE(.));
    } >itcm AT>main

    .text :
    {
        . = ALIGN(4);

        /* .text */
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
        *(.stub)
        *(.gnu.warning)
        *(.gnu.linkonce.t*)
        . = ALIGN(4);

        /* .fini */
        KEEP( *(.fini) )
        . = ALIGN(4);
    } >main

    .rodata :
    {
        *(.rodata)
        *(.roda)
        *(.rodata.*)
        *all.rodata*(*)
        *(.gnu.linkonce.r*)
        SORT(CONSTRUCTORS)
        . = ALIGN(4);
    } >main

    .preinit_array ALIGN(4) :
    {
        PROVIDE (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE (__preinit_array_end = .);
    } >main

    .init_array ALIGN(4) :
    {
        PROVIDE (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE (__init_array_end = .);
    } >main

    .fini_array ALIGN(4) :
    {
        PROVIDE (__fini_array_start = .);
        KEEP (*(.fini_array))
        KEEP (*(SORT(.fini_array.*)))
        PROVIDE (__fini_array_end = .);
    } >main

    .ctors ALIGN(4) :
    {
        KEEP (*crtbegin.o(.ctors)) /* MUST be first -- GCC requires it */
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
    } >main

    .dtors ALIGN(4) :
    {
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
    } >main

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >main
    __exidx_start = .;
    ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } >main
    __exidx_end = .;

    .data :
    {
        *(.data)
        *(.data.*)
        KEEP (*(.large_patch*))
        *(.gnu.linkonce.d*)
        CONSTRUCTORS
        . = ALIGN(4);
    } >main

    .bss :
    {
        . = ALIGN(32);
        PROVIDE (__bss_start__ = ABSOLUTE(.));
        *(.dynbss)
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b*)
        *(COMMON)
        . = ALIGN(8);
        PROVIDE (__bss_end__ = ABSOLUTE(.));
    } >main
    __end__ = ABSOLUTE(.) ;

    /* ==================
       ==== Metadata ====
       ================== */

    /* Discard sections that difficult post-processing */
    /DISCARD/ : { *(.group .comment .note) }

    /* Stabs debugging sections. */
    .stab          0 : { *(.stab) }
    .stabstr       0 : { *(.stabstr) }
    .stab.excl     0 : { *(.stab.excl) }
    .stab.exclstr  0 : { *(.stab.exclstr) }
    .stab.index    0 : { *(.stab.index) }
    .stab.indexstr 0 : { *(.stab.indexstr) }

    /* DWARF debug sections.
       Symbols in the DWARF debugging sections are relative to the beginning
       of the section so we begin them at 0. */

    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }

    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }

    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }

    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
}
