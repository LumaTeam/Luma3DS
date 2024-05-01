#pragma once

#include <3ds/types.h>

#define CONFIG(a)        (((config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((bootConfig >> (a)) & (b))

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

extern u32 config, multiConfig, bootConfig;

void readLumaConfig(void);
bool hasKExt(void);
u32 getKExtSize(void);
u32 getStolenSystemMemRegionSize(void);
bool isTitleLaunchPrevented(u64 titleId);
