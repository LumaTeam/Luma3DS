#pragma once

#include "types.h"

void memcpy(void *dest, const void *src, u32 size);

/***
    Cleans and invalidates the data cache, then waits for all memory transfers to be finished.
    This function MUST be called before doing the following:
        - rebooting
        - powering down
        - setting the ARM11 entrypoint to execute a function
        - jumping to a payload (?)
***/
void cleanInvalidateDCacheAndDMB(void);