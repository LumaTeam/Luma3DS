#pragma once

#include <3ds/types.h>

#define MAKE_BRANCH(src,dst)      (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define MAKE_BRANCH_LINK(src,dst) (0xEB000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

#define CONFIG(a)        (((info.config >> (a + 17)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 7)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)
#define LOADERFLAG(a)    ((info.flags >> (a + 4)) & 1) != 0

#define BOOTCFG_NAND         BOOTCONFIG(0, 7)
#define BOOTCFG_FIRM         BOOTCONFIG(3, 7)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(6, 1)

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU
};

enum singleOptions
{
    AUTOBOOTEMU = 0,
    USEEMUFIRM,
    LOADEXTFIRMSANDMODULES,
    PATCHGAMES,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PATCHACCESS,
    PATCHUNITINFO,
    ENABLEEXCEPTIONHANDLERS
};

enum flags
{
    ISN3DS = 0,
    ISSAFEMODE
};

void patchCode(u64 progId, u16 progVer, u8 *code, u32 size, u32 textSize, u32 roSize, u32 dataSize, u32 roAddress, u32 dataAddress);
