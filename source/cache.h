/*
*   cache.h
*       by TuxSH
*/

#pragma once
#include "types.h"

/***
    The following functions flush the data cache, then waits for all memory transfers to be finished.
    The data cache MUST be flushed before doing one of the following:
        - rebooting
        - powering down
        - setting the ARM11 entrypoint to execute a function
***/

void flushEntireDCache(void);
void flushDCacheRange(void *startAddress, u32 size);