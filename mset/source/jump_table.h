#ifndef __jump_table_h__
#define __jump_table_h__

extern void *jump_table __asm__("jump_table");
extern void *jump_table_end __asm__("jump_table_end");
extern void *jt_return __asm__("jt_return");
extern void *jt_pdn_regs __asm__("jt_pdn_regs");
extern void *jt_pxi_regs __asm__("jt_pxi_regs");

#endif
