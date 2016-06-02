.section .text.start
.align 4
.global _start
_start:
    add pc, r0, #(handlers - .) @ Dummy instruction to prevent compiler optimizations
    
handlers:
    .word FIQHandler
    .word undefinedInstructionHandler
    .word prefetchAbortHandler
    .word dataAbortHandler
