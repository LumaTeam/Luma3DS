#pragma once

#include <3ds/types.h>

#define MAKE_BRANCH(src,dst)      (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define MAKE_BRANCH_LINK(src,dst) (0xEB000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

#define CONFIG(a)        (((info.config >> (a + 20)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 8)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)
#define LOADERFLAG(a)    ((info.flags >> (a + 4)) & 1) != 0

#define BOOTCFG_NAND         BOOTCONFIG(0, 7)
#define BOOTCFG_FIRM         BOOTCONFIG(3, 7)
#define BOOTCFG_A9LH         BOOTCONFIG(6, 1)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(7, 1)

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU,
    DEVOPTIONS
};

enum singleOptions
{
    AUTOBOOTSYS = 0,
    USESYSFIRM,
    LOADEXTFIRMSANDMODULES,
    USECUSTOMPATH,
    PATCHGAMES,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PATCHACCESS
};

enum flags
{
    ISN3DS = 0,
    ISSAFEMODE
};

void patchCode(u64 progId, u16 progVer, u8 *code, u32 size);