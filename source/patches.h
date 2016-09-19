/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

/*
*   ARM11 modules patching code originally by Subv
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

typedef struct __attribute__((packed))
{
    char magic[4];
        
    u8 versionMajor;
    u8 versionMinor;
    u8 versionBuild;
    u8 flags;

    u32 commitHash;

    u32 config;
} CFWInfo;

extern bool isDevUnit;

u8 *getProcess9(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr);
u32 *getKernel11Info(u8 *pos, u32 size, u32 *baseK11VA, u8 **freeK11Space, u32 **arm11SvcHandler, u32 **arm11ExceptionsPage);
void patchSignatureChecks(u8 *pos, u32 size);
void patchTitleInstallMinVersionCheck(u8 *pos, u32 size);
void patchFirmlaunches(u8 *pos, u32 size, u32 process9MemAddr);
void patchFirmWrites(u8 *pos, u32 size);
void patchOldFirmWrites(u8 *pos, u32 size);
void reimplementSvcBackdoor(u8 *pos, u32 *arm11SvcTable, u32 baseK11VA, u8 **freeK11Space);
void implementSvcGetCFWInfo(u8 *pos, u32 *arm11SvcTable, u32 baseK11VA, u8 **freeK11Space);
void applyLegacyFirmPatches(u8 *pos, FirmwareType firmType);
void patchArm9ExceptionHandlersInstall(u8 *pos, u32 size);
u32 getInfoForArm11ExceptionHandlers(u8 *pos, u32 size, u32 *codeSetOffset);
void patchSvcBreak9(u8 *pos, u32 size, u32 kernel9Address);
void patchSvcBreak11(u8 *pos, u32 *arm11SvcTable);
void patchKernel9Panic(u8 *pos, u32 size);
void patchKernel11Panic(u8 *pos, u32 size);
void patchP9AccessChecks(u8 *pos, u32 size);
void patchArm11SvcAccessChecks(u32 *arm11SvcHandler);
void patchK11ModuleChecks(u8 *pos, u32 size, u8 **freeK11Space);
void patchUnitInfoValueSet(u8 *pos, u32 size);