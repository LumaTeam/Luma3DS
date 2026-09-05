#pragma once

#include <3ds/types.h>

// Returns false without modifying code when the SDK call or padding is unknown.
bool patchHomeMenuExefs(u8 *code, u32 size, u32 textSize, u32 reservedPadding);
