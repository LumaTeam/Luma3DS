#pragma once

#include <3ds/types.h>

#define CONFIG(a)        (((config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((bootConfig >> (a)) & (b))

#define BOOTCFG_NAND         BOOTCONFIG(0, 7)
#define BOOTCFG_FIRM         BOOTCONFIG(3, 7)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(6, 1)
#define BOOTCFG_NTRCARDBOOT  BOOTCONFIG(7, 1)

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU,
    AUTOBOOTMODE,
};

enum singleOptions
{
    AUTOBOOTEMU = 0,
    USEEMUFIRM,
    LOADEXTFIRMSANDMODULES,
    PATCHGAMES,
    REDIRECTAPPTHREADS,
    PATCHVERSTRING,
    SHOWGBABOOT,
    FORCEHEADPHONEOUTPUT,
    PATCHUNITINFO,
    DISABLEARM11EXCHANDLERS,
    ENABLESAFEFIRMROSALINA,
};

extern u32 config, multiConfig, bootConfig;

void readLumaConfig(void);
bool hasKExt(void);
u32 getKExtSize(void);
u32 getStolenSystemMemRegionSize(void);
bool isTitleLaunchPrevented(u64 titleId);
