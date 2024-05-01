#pragma once

#include "types.h"

#define MAKE_BRANCH(src,dst)      (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define MAKE_BRANCH_LINK(src,dst) (0xEB000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

#define CONFIG(a)        (((cfwInfo.config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((cfwInfo.multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((cfwInfo.bootConfig >> (a)) & (b))

#define BOOTCFG_NAND         BOOTCONFIG(0, 1)
#define BOOTCFG_EMUINDEX     BOOTCONFIG(1, 3)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(3, 1)
#define BOOTCFG_NTRCARDBOOT  BOOTCONFIG(4, 1)

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU,
    AUTOBOOTMODE,
    FORCEAUDIOOUTPUT,
};

enum singleOptions
{
    AUTOBOOTEMU = 0,
    LOADEXTFIRMSANDMODULES,
    PATCHGAMES,
    REDIRECTAPPTHREADS,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PATCHUNITINFO,
    ENABLEDSIEXTFILTER,
    DISABLEARM11EXCHANDLERS,
    ENABLESAFEFIRMROSALINA,
};
