#pragma once

#include <3ds/types.h>

u32 getKExtSize(void);
bool isTitleLaunchPrevented(u64 titleId);
Result fsRegSetupPermissionsForKip(u32 pid, u64 titleId);