// From: https://github.com/d0k3/GodMode9/blob/master/arm9/source/system/memmap.h

# pragma once


// general memory areas

#define __ITCM_ADDR     0x01FF8000
#define __ITCM_LEN      0x00008000

#define __DTCM_ADDR     0x30008000
#define __DTCM_LEN      0x00004000

#define __A9RAM0_ADDR   0x08000000
#define __A9RAM0_LEN    0x00100000

#define __A9RAM1_ADDR   0x08100000
#define __A9RAM1_LEN    0x00080000

#define __VRAM_ADDR     0x18000000
#define __VRAM_LEN      0x00600000

#define __DSP_ADDR      0x1FF00000
#define __DSP_LEN       0x00080000

#define __AWRAM_ADDR    0x1FF80000
#define __AWRAM_LEN     0x00080000

#define __OTP_ADDR      0x10012000
#define __OTP_LEN       0x00000100

#define __FCRAM0_ADDR   0x20000000
#define __FCRAM0_END    0x28000000
#define __FCRAM0_LEN    (__FCRAM0_END - __FCRAM0_ADDR)

#define __FCRAM1_ADDR   0x28000000
#define __FCRAM1_END    0x30000000
#define __FCRAM1_LEN    (__FCRAM1_END - __FCRAM1_ADDR)


// offsets provided by SciresM, only available if booted on b9s

#define __BOOT9_ADDR    0x08080000
#define __BOOT9_LEN     0x00010000

#define __BOOT11_ADDR   0x08090000
#define __BOOT11_LEN    0x00010000


// stuff in FCRAM
// FCRAM0 0x0000...0x1000 is unused
// see: https://www.3dbrew.org/wiki/FIRM#FIRM_Launch_Parameters

#define __FIRMRAM_ADDR  (__FCRAM0_ADDR + 0x0001000)
#define __FIRMRAM_END   (__FIRMRAM_ADDR + 0x0400000)

#define __FIRMTMP_ADDR  (__FCRAM0_END - 0x0800000)
#define __FIRMTMP_END   (__FIRMTMP_ADDR + 0x0400000)

#define __RAMDRV_ADDR   (__FCRAM0_ADDR + 0x2800000)
#define __RAMDRV_END    __FCRAM0_END
#define __RAMDRV_END_N  __FCRAM1_END

#define __STACK_ABT_TOP __RAMDRV_ADDR
#define __STACK_ABT_LEN 0x10000

#define __STACK_TOP     (__STACK_ABT_TOP - __STACK_ABT_LEN)
#define __STACK_LEN     0x7F0000

#define __HEAP_ADDR     (__FCRAM0_ADDR + 0x0001000)
#define __HEAP_END      (__STACK_TOP - __STACK_LEN)
