#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <3ds/types.h>

bool patcherApplyCodeBpsPatch(u64 progId, u8* code, u32 size);

#ifdef __cplusplus
}
#endif
