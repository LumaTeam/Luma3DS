#pragma once

#include <3ds/types.h>
#include <3ds/exheader.h>

Result pxipmInit(void);
void pxipmExit(void);
Result PXIPM_RegisterProgram(u64 *prog_handle, FS_ProgramInfo *title, FS_ProgramInfo *update);
Result PXIPM_GetProgramInfo(ExHeader *exheader, u64 prog_handle);
Result PXIPM_UnregisterProgram(u64 prog_handle);
