#pragma once

#include <3ds/types.h>

void mapFirmlaunchParameters(void);
Result GetFirmlaunchParams(void *outParams, size_t size);
Result SetFirmlaunchParams(const void *params, size_t size);
Result LaunchFirm(u32 firmTidLow, const void *params, size_t size);
