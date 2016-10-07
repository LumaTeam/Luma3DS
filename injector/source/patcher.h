#pragma once

#include <3ds/types.h>

#define CONFIG(a)        (((info.config >> (a + 21)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 9)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)

#define BOOTCFG_NAND         BOOTCONFIG(0, 7)
#define BOOTCFG_FIRM         BOOTCONFIG(3, 7)
#define BOOTCFG_A9LH         BOOTCONFIG(6, 1)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(7, 1)
#define BOOTCFG_SAFEMODE     BOOTCONFIG(8, 1)

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
    USELANGEMUANDCODE,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PATCHACCESS
};

void patchCode(u64 progId, u8 *code, u32 size);