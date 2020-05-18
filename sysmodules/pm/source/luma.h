#pragma once

#include <3ds/types.h>

bool hasKExt(void);
u32 getKExtSize(void);
u32 getStolenSystemMemRegionSize(void);
bool isTitleLaunchPrevented(u64 titleId);
