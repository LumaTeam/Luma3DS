/*
*   patches.h
*/

#pragma once

#include "types.h"

typedef struct patchData {
    u32 offset[2];
    union {
        u8 type0[8];
        u16 type1;
    } patch;
    u32 type;
} patchData;

u8 *getProcess9(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr);
void patchSignatureChecks(u8 *pos, u32 size);
void patchTitleInstallMinVersionCheck(u8 *pos, u32 size);
void patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr);
void patchFirmWrites(u8 *pos, u32 size);
void patchFirmWriteSafe(u8 *pos, u32 size);
void reimplementSvcBackdoor(u8 *pos, u32 size);
void applyLegacyFirmPatches(u8 *pos, FirmwareType firmType, bool isN3DS);
u32 getLoader(u8 *pos, u32 *loaderSize);