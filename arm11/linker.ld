OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

/* Mostly copied from https://github.com/devkitPro/buildscripts/blob/master/dkarm-eabi/crtls/3dsx.ld */

PHDRS
{
    main PT_LOAD;
}

SECTIONS
{
    PROVIDE(__start__           = 0x1FF80000);
    PROVIDE(__stack_top__       = 0x1FFFE000);
    PROVIDE(__stack_bottom__    = 0x1FFFD000);

    . = __start__;

    .text :
    {
        KEEP( *(.text.start) )
        KEEP( *(.init) )

        /* .text */
        *(.text)
        *(.text.*)
        *(.glue_7)
        *(.glue_7t)
        *(.stub)
        *(.gnu.warning)
        *(.gnu.linkonce.t*)

        /* .fini */
        KEEP( *(.fini) )
        . = ALIGN(32);
    } :main

    .rodata :
    {
        *(.rodata)
        *(.roda)
        *(.rodata.*)
        *all.rodata*(*)
        *(.gnu.linkonce.r*)
        SORT(CONSTRUCTORS)
        . = ALIGN(8);
    }

    .preinit_array :
    {
        PROVIDE (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE (__preinit_array_end = .);
    }

    .init_array :
    {
        PROVIDE (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE (__init_array_end = .);
    }

    .fini_array :
    {
        PROVIDE (__fini_array_start = .);
        KEEP (*(.fini_array))
        KEEP (*(SORT(.fini_array.*)))
        PROVIDE (__fini_array_end = .);
    }

    .ctors :
    {
        KEEP (*crtbegin.o(.ctors)) /* MUST be first -- GCC requires it */
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        . = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
    }

    .dtors :
    {
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
        . = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
    }

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) __exidx_start = .;}
    ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*)  __exidx_end = .;}

    .data :
    {
        *(.data)
        *(.data.*)
        KEEP (*(.large_patch*))
        *(.gnu.linkonce.d*)
        CONSTRUCTORS
        . = ALIGN(32);
    }

    .bss (NOLOAD) :
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
    } :NONE

    PROVIDE (__end__ = ABSOLUTE(.));

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
