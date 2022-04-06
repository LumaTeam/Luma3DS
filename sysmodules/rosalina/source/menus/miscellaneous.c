/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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
#include "menus/miscellaneous.h"
#include "input_redirection.h"
#include "ntp.h"
#include "memory.h"
#include "draw.h"
#include "hbloader.h"
#include "fmt.h"
#include "utils.h" // for makeArmBranch
#include "minisoc.h"
#include "ifile.h"
#include "pmdbgext.h"
#include "plugin.h"
#include "process_patches.h"
#include "screen_filters.h"
#include "config_template_ini.h"

#define CONFIG(a)        (((cfg->config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((cfg->multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((cfg->bootConfig >> (a)) & (b))

enum singleOptions
{
    AUTOBOOTEMU = 0,
    USEEMUFIRM,
    LOADEXTFIRMSANDMODULES,
    PATCHGAMES,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PATCHUNITINFO,
    DISABLEARM11EXCHANDLERS,
    ENABLESAFEFIRMROSALINA,
};

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU
};
typedef struct DspFirmSegmentHeader {
    u32 offset;
    u32 loadAddrHalfwords;
    u32 size;
    u8 _0x0C[3];
    u8 memType;
    u8 hash[0x20];
} DspFirmSegmentHeader;

typedef struct DspFirm {
    u8 signature[0x100];
    char magic[4];
    u32 totalSize; // no more than 0x10000
    u16 layoutBitfield;
    u8 _0x10A[3];
    u8 surroundSegmentMemType;
    u8 numSegments; // no more than 10
    u8 flags;
    u32 surroundSegmentLoadAddrHalfwords;
    u32 surroundSegmentSize;
    u8 _0x118[8];
    DspFirmSegmentHeader segmentHdrs[10];
    u8 data[];
} DspFirm;

typedef struct CfgData {
    u16 formatVersionMajor, formatVersionMinor;

    u32 config, multiConfig, bootConfig;
    u32 splashDurationMsec;

    u64 hbldr3dsxTitleId;
    u32 rosalinaMenuCombo;
    u32 pluginLoaderFlags;
    u16 screenFiltersCct;
    s16 ntpTzOffetMinutes;
} CfgData;

Menu miscellaneousMenu = {
    "Miscellaneous options menu",
    {
        { "Switch the hb. title to the current app.", METHOD, .method = &MiscellaneousMenu_SwitchBoot3dsxTargetTitle },
        { "Change the menu combo", METHOD, .method = &MiscellaneousMenu_ChangeMenuCombo },
        { "Start InputRedirection", METHOD, .method = &MiscellaneousMenu_InputRedirection },
        { "Update time and date via NTP", METHOD, .method = &MiscellaneousMenu_UpdateTimeDateNtp },
        { "Nullify user time offset", METHOD, .method = &MiscellaneousMenu_NullifyUserTimeOffset },
        { "Dump DSP firmware", METHOD, .method = &MiscellaneousMenu_DumpDspFirm },
        { "Save settings", METHOD, .method = &MiscellaneousMenu_SaveSettings },
        {},
    }
};
int lastNtpTzOffset = 0;
bool saveSettingsRequest = false;

void MiscellaneousMenu_SwitchBoot3dsxTargetTitle(void)
{
    Result res;
    char failureReason[64];

    if(Luma_SharedConfig->hbldr_3dsx_tid == HBLDR_DEFAULT_3DSX_TID)
    {
        FS_ProgramInfo progInfo;
        u32 pid;
        u32 launchFlags;
        res = PMDBG_GetCurrentAppInfo(&progInfo, &pid, &launchFlags);
        if(R_SUCCEEDED(res))
        {
            Luma_SharedConfig->hbldr_3dsx_tid = progInfo.programId;
            miscellaneousMenu.items[0].title = "Switch the hb. title to hblauncher_loader";
        }
        else
        {
            res = -1;
            strcpy(failureReason, "no suitable process found");
        }
    }
    else
    {
        res = 0;
        Luma_SharedConfig->hbldr_3dsx_tid = HBLDR_DEFAULT_3DSX_TID;
        miscellaneousMenu.items[0].title = "Switch the hb. title to the current app.";
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (%s).", failureReason);

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

static void MiscellaneousMenu_ConvertComboToString(char *out, u32 combo)
{
    static const char *keys[] = {
        "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y",
        "?", "?",
        "ZL", "ZR",
        "?", "?", "?", "?",
        "Touch",
        "?", "?", "?",
        "CStick Right", "CStick Left", "CStick Up", "CStick Down",
        "CPad Right", "CPad Left", "CPad Up", "CPad Down",
    };

    char *outOrig = out;
    out[0] = 0;
    for(s32 i = 31; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keys[i]);
            out += strlen(keys[i]);
            *out++ = '+';
        }
    }

    if (out != outOrig)
        out[-1] = 0;
}

void MiscellaneousMenu_ChangeMenuCombo(void)
{
    char comboStrOrig[128], comboStr[128];
    u32 posY;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    MiscellaneousMenu_ConvertComboToString(comboStrOrig, menuCombo);

    Draw_Lock();
    Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

    posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "The current menu combo is:  %s", comboStrOrig);
    posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Please enter the new combo:");

    menuCombo = waitCombo();
    MiscellaneousMenu_ConvertComboToString(comboStr, menuCombo);

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "The current menu combo is:  %s", comboStrOrig);
        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Please enter the new combo: %s", comboStr) + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Successfully changed the menu combo.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

static size_t saveLumaIniConfigToStr(char *out, const CfgData *cfg)
{
    char lumaVerStr[64];
    char lumaRevSuffixStr[16];
    char rosalinaMenuComboStr[128];

    const char *splashPosStr;
    const char *n3dsCpuStr;

    s64 outInfo;
    svcGetSystemInfo(&outInfo, 0x10000, 0);
    u32 version = (u32)outInfo;

    svcGetSystemInfo(&outInfo, 0x10000, 1);
    u32 commitHash = (u32)outInfo;

    svcGetSystemInfo(&outInfo, 0x10000, 0x200);
    bool isRelease = (bool)outInfo;

    switch (MULTICONFIG(SPLASH)) {
        default: case 0: splashPosStr = "off"; break;
        case 1: splashPosStr = "before payloads"; break;
        case 2: splashPosStr = "after payloads"; break;
    }

    switch (MULTICONFIG(NEWCPU)) {
        default: case 0: n3dsCpuStr = "off"; break;
        case 1: n3dsCpuStr = "clock"; break;
        case 2: n3dsCpuStr = "l2"; break;
        case 3: n3dsCpuStr = "clock+l2"; break;
    }

    if (GET_VERSION_REVISION(version) != 0) {
        sprintf(lumaVerStr, "Luma3DS v%d.%d.%d", (int)GET_VERSION_MAJOR(version), (int)GET_VERSION_MINOR(version), (int)GET_VERSION_REVISION(version));
    } else {
        sprintf(lumaVerStr, "Luma3DS v%d.%d",  (int)GET_VERSION_MAJOR(version), (int)GET_VERSION_MINOR(version));
    }

    if (isRelease) {
        strcpy(lumaRevSuffixStr, "");
    } else {
        sprintf(lumaRevSuffixStr, "-%08lx", (u32)commitHash);
    }

    MiscellaneousMenu_ConvertComboToString(rosalinaMenuComboStr, cfg->rosalinaMenuCombo);

    static const int pinOptionToDigits[] = { 0, 4, 6, 8 };
    int pinNumDigits = pinOptionToDigits[MULTICONFIG(PIN)];

    int n = sprintf(
        out, (const char *)config_template_ini,
        lumaVerStr, lumaRevSuffixStr,

        (int)cfg->formatVersionMajor, (int)cfg->formatVersionMinor,
        (int)CONFIG(AUTOBOOTEMU), (int)CONFIG(USEEMUFIRM),
        (int)CONFIG(LOADEXTFIRMSANDMODULES), (int)CONFIG(PATCHGAMES),
        (int)CONFIG(PATCHVERSTRING), (int)CONFIG(SHOWGBABOOT),

        1 + (int)MULTICONFIG(DEFAULTEMU), 4 - (int)MULTICONFIG(BRIGHTNESS),
        splashPosStr, (unsigned int)cfg->splashDurationMsec,
        pinNumDigits, n3dsCpuStr,

        cfg->hbldr3dsxTitleId, rosalinaMenuComboStr, (int)(cfg->pluginLoaderFlags & 1),
        (int)cfg->screenFiltersCct, (int)cfg->ntpTzOffetMinutes,

        (int)CONFIG(PATCHUNITINFO), (int)CONFIG(DISABLEARM11EXCHANDLERS),
        (int)CONFIG(ENABLESAFEFIRMROSALINA)
    );

    return n < 0 ? 0 : (size_t)n;
}

void RequestSaveSettings(void) {
    saveSettingsRequest = true;
}

Result  SaveSettings(void)
{
    char inibuf[0x2000];

    Result res;

    IFile file;
    u64 total;

    CfgData configData;

    u32 formatVersion;
    u32 config, multiConfig, bootConfig;
    u32 splashDurationMsec;

    s64 out;
    bool isSdMode;
    svcGetSystemInfo(&out, 0x10000, 2);
    formatVersion = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 3);
    config = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 4);
    multiConfig = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 5);
    bootConfig = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 6);
    splashDurationMsec = (u32)out;
    svcGetSystemInfo(&out, 0x10000, 0x203);
    isSdMode = (bool)out;

    configData.formatVersionMajor = (u16)(formatVersion >> 16);
    configData.formatVersionMinor = (u16)formatVersion;
    configData.config = config;
    configData.multiConfig = multiConfig;
    configData.bootConfig = bootConfig;
    configData.splashDurationMsec = splashDurationMsec;
    configData.hbldr3dsxTitleId = Luma_SharedConfig->hbldr_3dsx_tid;
    configData.rosalinaMenuCombo = menuCombo;
    configData.pluginLoaderFlags = PluginLoader__IsEnabled();
    configData.screenFiltersCct = (u16)screenFiltersCurrentTemperature;
    configData.ntpTzOffetMinutes = (s16)lastNtpTzOffset;

    size_t n = saveLumaIniConfigToStr(inibuf, &configData);
    FS_ArchiveID archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    if (n > 0)
        res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/config.ini"), FS_OPEN_CREATE | FS_OPEN_WRITE);
    else
        res = -1;

    if(R_SUCCEEDED(res))
        res = IFile_SetSize(&file, n);
    if(R_SUCCEEDED(res))
        res = IFile_Write(&file, &total, inibuf, n, 0);
    IFile_Close(&file);

    IFile_Close(&file);
    return res;
}

void MiscellaneousMenu_SaveSettings(void)
{
    Result res = SaveSettings();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void MiscellaneousMenu_InputRedirection(void)
{
    bool done = false;

    Result res;
    char buf[65];
    bool wasEnabled = inputRedirectionEnabled;
    bool cantStart = false;

    if(wasEnabled)
    {
        res = InputRedirection_Disable(5 * 1000 * 1000 * 1000LL);
        if(res != 0)
            sprintf(buf, "Failed to stop InputRedirection (0x%08lx).", (u32)res);
        else
            miscellaneousMenu.items[2].title = "Start InputRedirection";
    }
    else
    {
        s64     dummyInfo;
        bool    isN3DS = svcGetSystemInfo(&dummyInfo, 0x10001, 0) == 0;
        bool    isSocURegistered;

        res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
        cantStart = R_FAILED(res) || !isSocURegistered;

        if(!cantStart && isN3DS)
        {
            bool    isIrRstRegistered;

            res = srvIsServiceRegistered(&isIrRstRegistered, "ir:rst");
            cantStart = R_FAILED(res) || !isIrRstRegistered;
        }
    }

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        if(!wasEnabled && cantStart)
            Draw_DrawString(10, 30, COLOR_WHITE, "Can't start the input redirection before the system\nhas finished loading.");
        else if(!wasEnabled)
        {
            Draw_DrawString(10, 30, COLOR_WHITE, "Starting InputRedirection...");
            if(!done)
            {
                res = InputRedirection_DoOrUndoPatches();
                if(R_SUCCEEDED(res))
                {
                    res = svcCreateEvent(&inputRedirectionThreadStartedEvent, RESET_STICKY);
                    if(R_SUCCEEDED(res))
                    {
                        inputRedirectionCreateThread();
                        res = svcWaitSynchronization(inputRedirectionThreadStartedEvent, 10 * 1000 * 1000 * 1000LL);
                        if(res == 0)
                            res = (Result)inputRedirectionStartResult;

                        if(res != 0)
                        {
                            svcCloseHandle(inputRedirectionThreadStartedEvent);
                            InputRedirection_DoOrUndoPatches();
                            inputRedirectionEnabled = false;
                        }
                        inputRedirectionStartResult = 0;
                    }
                }

                if(res != 0)
                    sprintf(buf, "Starting InputRedirection... failed (0x%08lx).", (u32)res);
                else
                    miscellaneousMenu.items[2].title = "Stop InputRedirection";

                done = true;
            }

            if(res == 0)
                Draw_DrawString(10, 30, COLOR_WHITE, "Starting InputRedirection... OK.");
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }
        else
        {
            if(res == 0)
            {
                u32 posY = 30;
                posY = Draw_DrawString(10, posY, COLOR_WHITE, "InputRedirection stopped successfully.\n\n");
                if (isN3DS)
                {
                    posY = Draw_DrawString(
                        10,
                        posY,
                        COLOR_WHITE,
                        "This might cause a key press to be repeated in\n"
                        "Home Menu for no reason.\n\n"
                        "Just pressing ZL/ZR on the console is enough to fix\nthis.\n"
                    );
                }
            }
            else
                Draw_DrawString(10, 30, COLOR_WHITE, buf);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

void MiscellaneousMenu_UpdateTimeDateNtp(void)
{
    u32 posY;
    u32 input = 0;

    Result res;
    bool cantStart = false;

    bool isSocURegistered;

    u64 msSince1900, samplingTick;

    res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
    cantStart = R_FAILED(res) || !isSocURegistered;

    int dt = 12*60 + lastNtpTzOffset;
    int utcOffset = dt / 60;
    int utcOffsetMinute = dt%60;
    int absOffset;
    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        absOffset = utcOffset - 12;
        absOffset = absOffset < 0 ? -absOffset : absOffset;
        posY = Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Current UTC offset:  %c%02d%02d", utcOffset < 12 ? '-' : '+', absOffset, utcOffsetMinute);
        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Use DPAD Left/Right to change hour offset.\nUse DPAD Up/Down to change minute offset.\nPress A when done.") + SPACING_Y;

        input = waitInput();

        if(input & KEY_LEFT) utcOffset = (27 + utcOffset - 1) % 27; // ensure utcOffset >= 0
        if(input & KEY_RIGHT) utcOffset = (utcOffset + 1) % 27;
        if(input & KEY_UP) utcOffsetMinute = (utcOffsetMinute + 1) % 60;
        if(input & KEY_DOWN) utcOffsetMinute = (60 + utcOffsetMinute - 1) % 60;
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(input & (KEY_A | KEY_B)) && !menuShouldExit);

    if (input & KEY_B)
        return;

    utcOffset -= 12;
    lastNtpTzOffset = 60 * utcOffset + utcOffsetMinute;

    res = srvIsServiceRegistered(&isSocURegistered, "soc:U");
    cantStart = R_FAILED(res) || !isSocURegistered;
    res = 0;
    if(!cantStart)
    {
        res = ntpGetTimeStamp(&msSince1900, &samplingTick);
        if(R_SUCCEEDED(res))
        {
            msSince1900 += 1000 * (3600 * utcOffset + 60 * utcOffsetMinute);
            res = ntpSetTimeDate(msSince1900, samplingTick);
        }
    }

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");

        absOffset = utcOffset;
        absOffset = absOffset < 0 ? -absOffset : absOffset;
        Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Current UTC offset:  %c%02d", utcOffset < 0 ? '-' : '+', absOffset);
        if (cantStart)
            Draw_DrawFormattedString(10, posY + 2 * SPACING_Y, COLOR_WHITE, "Can't sync time/date before the system\nhas finished loading.") + SPACING_Y;
        else if (R_FAILED(res))
            Draw_DrawFormattedString(10, posY + 2 * SPACING_Y, COLOR_WHITE, "Operation failed (%08lx).", (u32)res) + SPACING_Y;
        else
            Draw_DrawFormattedString(10, posY + 2 * SPACING_Y, COLOR_WHITE, "Time/date updated successfully.") + SPACING_Y;

        input = waitInput();

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(input & KEY_B) && !menuShouldExit);

}

void MiscellaneousMenu_NullifyUserTimeOffset(void)
{
    Result res = ntpNullifyUserTimeOffset();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.\n\nPlease reboot to finalize the changes.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}

static Result MiscellaneousMenu_DumpDspFirmCallback(Handle procHandle, u32 textSz, u32 roSz, u32 rwSz)
{
    (void)procHandle;
    Result res = 0;

    // NOTE: we suppose .text, .rodata, .data+.bss are contiguous & in that order
    u32 rwStart = 0x00100000 + textSz + roSz;
    u32 rwEnd = rwStart + rwSz;

    // Locate the DSP firm (it's in .data, not .rodata, suprisingly)
    u32 magic;
    memcpy(&magic, "DSP1", 4);
    const u32 *off = (u32 *)rwStart;

    for (; off < (u32 *)rwEnd && *off != magic; off++);

    if (off >= (u32 *)rwEnd || off < (u32 *)(rwStart + 0x100))
        return -2;

    // Do some sanity checks
    const DspFirm *firm = (const DspFirm *)((u32)off - 0x100);
    if (firm->totalSize > 0x10000 || firm->numSegments > 10)
        return -3;
    if ((u32)firm + firm->totalSize >= rwEnd)
        return -3;

    // Dump to SD card (no point in dumping to CTRNAND as 3dsx stuff doesn't work there)
    IFile file;
    res = IFile_Open(
        &file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
        fsMakePath(PATH_ASCII, "/3ds/dspfirm.cdc"), FS_OPEN_CREATE | FS_OPEN_WRITE
    );

    u64 total;
    if(R_SUCCEEDED(res))
        res = IFile_Write(&file, &total, firm, firm->totalSize, 0);
    if(R_SUCCEEDED(res))
        res = IFile_SetSize(&file, firm->totalSize); // truncate accordingly

    IFile_Close(&file);

    return res;
}
void MiscellaneousMenu_DumpDspFirm(void)
{
    Result res = OperateOnProcessByName("menu", MiscellaneousMenu_DumpDspFirmCallback);

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Miscellaneous options menu");
        if(R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "DSP firm. successfully written to /3ds/dspfirm.cdc\non the SD card.");
        else
            Draw_DrawFormattedString(
                10, 30, COLOR_WHITE,
                "Operation failed (0x%08lx).\n\nMake sure that Home Menu is running and that your\nSD card is inserted.",
                res
            );
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & KEY_B) && !menuShouldExit);
}
