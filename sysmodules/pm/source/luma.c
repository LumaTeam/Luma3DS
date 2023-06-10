#include <3ds.h>
#include <string.h>
#include "luma.h"
#include "util.h"

u32 config, multiConfig, bootConfig;

void readLumaConfig(void)
{
    s64 out = 0;
    bool isLumaWithKext = svcGetSystemInfo(&out, 0x20000, 0) == 1;
    if (isLumaWithKext)
    {
        svcGetSystemInfo(&out, 0x10000, 3);
        config = (u32)out;
        svcGetSystemInfo(&out, 0x10000, 4);
        multiConfig = (u32)out;
        svcGetSystemInfo(&out, 0x10000, 5);
        bootConfig = (u32)out;
    }
}

bool hasKExt(void)
{
    s64 val;
    return svcGetSystemInfo(&val, 0x20000, 0) == 1;
}

u32 getKExtSize(void)
{
    s64 val;
    svcGetSystemInfo(&val, 0x10000, 0x300);
    return (u32)val;
}

u32 getStolenSystemMemRegionSize(void)
{
    s64 val;
    svcGetSystemInfo(&val, 0x10000, 0x301);
    return (u32)val;
}

bool isTitleLaunchPrevented(u64 titleId)
{
    s64 numKips = 0;

    svcGetSystemInfo(&numKips, 26, 0);
    return numKips >= 6 && (titleId & ~(N3DS_TID_MASK | 1)) == 0x0004003000008A02ULL; // ErrDisp
}
