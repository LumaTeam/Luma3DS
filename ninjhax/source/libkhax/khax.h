#pragma once

#include <3ds.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize and do the initial pwning of the ARM11 kernel.
Result khaxInit();
// Shut down libkhax
Result khaxExit();

#ifdef __cplusplus
}
#endif
