@
@   cache.s
@       by TuxSH
@
@   This is part of Luma3DS, see LICENSE.txt for details
@

.arm
.global flushCaches
.type   flushCaches STT_FUNC

flushCaches:
    @ Clean and flush data cache
    @ Adpated from http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0155a/ch03s03s05.html ,
    @ and https://github.com/gemarcano/libctr9_io/blob/master/src/ctr_system_ARM.c#L39 as well
    @ Note: ARM's example is actually for a 8KB DCache (which is what the 3DS has)
    @ Implemented in bootROM at address 0xffff0830

    mov r1, #0                          @ segment counter
    outer_loop:
        mov r0, #0                      @ line counter

        inner_loop:
            orr r2, r1, r0                  @ generate segment and line address
            mcr p15, 0, r2, c7, c14, 2      @ clean and flush the line
            add r0, #0x20                   @ increment to next line
            cmp r0, #0x400
            bne inner_loop

        add r1, #0x40000000
        cmp r1, #0
        bne outer_loop

    mcr p15, 0, r1, c7, c10, 4              @ drain write buffer

    @ Flush instruction cache
    mcr p15, 0, r1, c7, c5, 0

    bx lr
