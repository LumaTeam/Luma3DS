/*
*   cache.h
*       by TuxSH
*/

#pragma once

#include "types.h"

/***
    The following functions flush the data cache, then waits for all memory transfers to be finished.
    The data cache and/or the instruction cache MUST be flushed before doing one of the following:
        - rebooting
        - powering down
        - setting the ARM11 entrypoint to execute a function
        - jumping to a payload
***/

void flushEntireDCache(void); //actually: "clean and flush"
void flushDCacheRange(void *startAddress, u32 size);
void flushEntireICache(void);
void flushICacheRange(void *startAddress, u32 size);