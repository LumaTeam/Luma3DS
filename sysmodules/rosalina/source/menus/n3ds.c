/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include "fmt.h"
#include "menus/n3ds.h"
#include "memory.h"
#include "menu.h"
#include "menus.h"
#include "ifile.h"
#include "pmdbgext.h"

static char clkRateBuf[128 + 1], new3dsMenuBuf[128 + 1], new3dsMenuConfigBuf[128 + 1], fileNameBuf[64];

Menu N3DSMenu = {
    "New 3DS menu",
    {
        { "Enable L2 cache", METHOD, .method = &N3DSMenu_EnableDisableL2Cache },
        { clkRateBuf, METHOD, .method = &N3DSMenu_ChangeClockRate },
        { new3dsMenuConfigBuf, METHOD, .method = &N3DSMenu_UpdateConfig, .visibility = &currentTitleAvailable },
        {},
    }
};

static s64 clkRate = 0, higherClkRate = 0, L2CacheEnabled = 0;
static u64 programId = 0;
static bool currentTitleUseN3DS = false;

bool currentTitleAvailable(void)
{
    return programId > 0;
}

void N3DSMenu_UpdateStatus(void)
{
    svcGetSystemInfo(&clkRate, 0x10001, 0);
    svcGetSystemInfo(&higherClkRate, 0x10001, 1);
    svcGetSystemInfo(&L2CacheEnabled, 0x10001, 2);

    N3DSMenu.items[0].title = L2CacheEnabled ? "Disable L2 cache" : "Enable L2 cache";
    sprintf(clkRateBuf, "Set clock rate to %luMHz", clkRate != 268 ? 268 : (u32)higherClkRate);

    sprintf(new3dsMenuBuf, "New 3DS settings: [%luMHz%s", clkRate == 268 ? 268 : (u32)higherClkRate, L2CacheEnabled ? " & L2]" : "]");

    rosalinaMenu.items[4].title = new3dsMenuBuf;
}

void N3DSMenu_ChangeClockRate(void)
{
    N3DSMenu_UpdateStatus();

    s64 newBitMask = (L2CacheEnabled << 1) | ((clkRate != 268 ? 1 : 0) ^ 1);
    svcKernelSetState(10, (u32)newBitMask);

    N3DSMenu_UpdateStatus();
}

void N3DSMenu_EnableDisableL2Cache(void)
{
    N3DSMenu_UpdateStatus();

    s64 newBitMask = ((L2CacheEnabled ^ 1) << 1) | (clkRate != 268 ? 1 : 0);
    svcKernelSetState(10, (u32)newBitMask);

    N3DSMenu_UpdateStatus();
}

void N3DSMenu_CheckForConfigFile(void)
{
    programId = 0;
    currentTitleUseN3DS = false;

    FS_ProgramInfo programInfo;
    u32 pid;
    u32 launchFlags;

    if(R_SUCCEEDED(PMDBG_GetCurrentAppInfo(&programInfo, &pid, &launchFlags)))
    {
        programId = programInfo.programId;

        sprintf(fileNameBuf, "%s%016llX%s", "/luma/n3ds/", programId, ".bin");
        IFile file;

        if(R_SUCCEEDED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),fsMakePath(PATH_ASCII, fileNameBuf), FS_OPEN_READ)))
        {
            IFile_Close(&file);
            currentTitleUseN3DS = true;
        }
    }

    N3DSMenu_UpdateConfigStatus();
}

void N3DSMenu_CreateConfigFile(void)
{
    IFile file;
  
     if(R_SUCCEEDED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),fsMakePath(PATH_ASCII, fileNameBuf), FS_OPEN_CREATE | FS_OPEN_WRITE)))
     {
         IFile_Close(&file);
         currentTitleUseN3DS = true;
     }
}

void N3DSMenu_DeleteConfigFile(void)
{
    FS_Archive archive;

    if(R_SUCCEEDED(FSUSER_OpenArchive(&archive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        if(R_SUCCEEDED(FSUSER_DeleteFile(archive, fsMakePath(PATH_ASCII, fileNameBuf))))
        {
            currentTitleUseN3DS = false;
        }
    
        FSUSER_CloseArchive(archive);
    }
}

void N3DSMenu_UpdateConfig(void)
{
    currentTitleUseN3DS ? N3DSMenu_DeleteConfigFile() : N3DSMenu_CreateConfigFile();
    N3DSMenu_UpdateConfigStatus();
}

void N3DSMenu_UpdateConfigStatus(void)
{
    sprintf(new3dsMenuConfigBuf, "Auto run current title as N3DS: %s", currentTitleUseN3DS ? "[true]" : "[false]");
}
