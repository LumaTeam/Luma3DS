#pragma once

#include <3ds/types.h>

// Used by the custom loader command 0x101 (ControlApplicationMemoryModeOverride)
typedef struct ControlApplicationMemoryModeOverrideConfig {
    u32 query : 1; //< Only query the current configuration, do not update it.
    u32 enable_o3ds : 1; //< Enable o3ds memory mode override
    u32 enable_n3ds : 1; //< Enable n3ds memory mode override
    u32 o3ds_mode : 3; //< O3ds memory mode
    u32 n3ds_mode : 3; //< N3ds memory mode
} ControlApplicationMemoryModeOverrideConfig;

void loaderHandleCommands(void *ctx);
