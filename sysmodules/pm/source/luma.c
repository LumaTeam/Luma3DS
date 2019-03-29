#include <3ds.h>
#include <string.h>
#include "luma.h"
#include "util.h"

u32 getKExtSize(void)
{
    s64 val;
    Result res = svcGetSystemInfo(&val, 0x10000, 0x300);
    return R_FAILED(res) ? 0 : (u32)val;
}

bool isTitleLaunchPrevented(u64 titleId)
{
    s64 numKips = 0;

    svcGetSystemInfo(&numKips, 26, 0);
    return numKips >= 6 && (titleId & ~N3DS_TID_BIT) == 0x0004003000008A02ULL; // ErrDisp
}