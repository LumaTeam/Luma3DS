#pragma once

#include "utils.h"
#include "kernel.h"
#include "svc.h"

/// Operations for svcControlProcess
typedef enum ProcessOp
{
    PROCESSOP_GET_ALL_HANDLES,  ///< List all handles of the process, varg3 can be either 0 to fetch all handles, or token of the type to fetch
                                ///< svcControlProcess(handle, PROCESSOP_GET_ALL_HANDLES, (u32)&outBuf, 0)
    PROCESSOP_SET_MMU_TO_RWX,   ///< Set the whole memory of the process with rwx access
                                ///< svcControlProcess(handle, PROCESSOP_SET_MMU_TO_RWX, 0, 0)
    PROCESSOP_GET_ON_MEMORY_CHANGE_EVENT,
    PROCESSOP_GET_ON_EXIT_EVENT,
    PROCESSOP_GET_PA_FROM_VA,   ///< Get the physical address of the va within the process
                                ///< svcControlProcess(handle, PROCESSOP_GET_PA_FROM_VA, (u32)&outPa, va)
    PROCESSOP_SCHEDULE_THREADS,
} ProcessOp;

Result  ControlProcess(Handle process, ProcessOp op, u32 varg2, u32 varg3);
