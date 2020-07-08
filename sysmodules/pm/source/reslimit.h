#pragma once

#include <3ds/svc.h>

Result initializeReslimits(void);
Result setAppMemLimit(u32 limit);
Result resetAppMemLimit(void);
Result setAppCpuTimeLimit(s64 limit);
void setAppCpuTimeLimitAndSchedModeFromDescriptor(u64 titleId, u16 descriptor);

Result SetAppResourceLimit(u32 mbz, ResourceLimitType category, u32 value, u64 mbz2);
Result GetAppResourceLimit(s64 *value, u32 mbz, ResourceLimitType category, u32 mbz2, u64 mbz3);
