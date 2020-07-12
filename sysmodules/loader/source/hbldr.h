#pragma once

#include <3ds/exheader.h>

Result hbldrInit(void);
void hbldrExit(void);

Result HBLDR_LoadProcess(Handle *outCodeSet, u32 textAddr, u32 kernelFlags, u64 titleId, const char *name);
Result HBLDR_SetTarget(const char* path);
Result HBLDR_SetArgv(const void* buffer, size_t size);
Result HBLDR_PatchExHeaderInfo(ExHeader_Info *exheaderInfo);
Result HBLDR_DebugNextApplicationByForce(bool dryRun);
