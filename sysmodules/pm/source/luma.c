#include <3ds.h>
#include <string.h>
#include "luma.h"
#include "util.h"

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
