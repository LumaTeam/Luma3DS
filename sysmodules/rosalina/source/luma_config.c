/*
*   This file is part of Luma3DS
*   Copyright (C) 2023 Aurora Wright, TuxSH
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
#include "memory.h"
#include "fmt.h"
#include "luma_config.h"
#include "screen_filters.h"
#include "config_template_ini.h"
#include "ifile.h"
#include "menus/miscellaneous.h"

typedef struct CfgData {
    u16 formatVersionMajor, formatVersionMinor;

    u32 config, multiConfig, bootConfig;
    u32 splashDurationMsec;

    u64 hbldr3dsxTitleId;
    u32 rosalinaMenuCombo;
    s16 ntpTzOffetMinutes;

    ScreenFilter topScreenFilter;
    ScreenFilter bottomScreenFilter;

    u64 autobootTwlTitleId;
    u8 autobootCtrAppmemtype;
} CfgData;

void LumaConfig_ConvertComboToString(char *out, u32 combo)
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

static size_t LumaConfig_SaveLumaIniConfigToStr(char *out, const CfgData *cfg)
{
    char lumaVerStr[64];
    char lumaRevSuffixStr[16];
    char rosalinaMenuComboStr[128];

    const char *splashPosStr;
    const char *n3dsCpuStr;
    const char *autobootModeStr;
    const char *forceAudioOutputStr;

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

    switch (MULTICONFIG(AUTOBOOTMODE)) {
        default: case 0: autobootModeStr = "off"; break;
        case 1: autobootModeStr = "3ds"; break;
        case 2: autobootModeStr = "dsi"; break;
    }

    switch (MULTICONFIG(FORCEAUDIOOUTPUT)) {
        default: case 0: forceAudioOutputStr = "off"; break;
        case 1: forceAudioOutputStr = "headphones"; break;
        case 2: forceAudioOutputStr = "speakers"; break;
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

    LumaConfig_ConvertComboToString(rosalinaMenuComboStr, cfg->rosalinaMenuCombo);

    static const int pinOptionToDigits[] = { 0, 4, 6, 8 };
    int pinNumDigits = pinOptionToDigits[MULTICONFIG(PIN)];

    char topScreenFilterGammaStr[32];
    char topScreenFilterContrastStr[32];
    char topScreenFilterBrightnessStr[32];
    floatToString(topScreenFilterGammaStr, cfg->topScreenFilter.gamma, 6, false);
    floatToString(topScreenFilterContrastStr, cfg->topScreenFilter.contrast, 6, false);
    floatToString(topScreenFilterBrightnessStr, cfg->topScreenFilter.brightness, 6, false);

    char bottomScreenFilterGammaStr[32];
    char bottomScreenFilterContrastStr[32];
    char bottomScreenFilterBrightnessStr[32];
    floatToString(bottomScreenFilterGammaStr, cfg->bottomScreenFilter.gamma, 6, false);
    floatToString(bottomScreenFilterContrastStr, cfg->bottomScreenFilter.contrast, 6, false);
    floatToString(bottomScreenFilterBrightnessStr, cfg->bottomScreenFilter.brightness, 6, false);

    int n = sprintf(
        out, (const char *)config_template_ini,
        lumaVerStr, lumaRevSuffixStr,

        (int)cfg->formatVersionMajor, (int)cfg->formatVersionMinor,
        (int)CONFIG(AUTOBOOTEMU), (int)CONFIG(USEEMUFIRM),
        (int)CONFIG(LOADEXTFIRMSANDMODULES), (int)CONFIG(PATCHGAMES),
        (int)CONFIG(REDIRECTAPPTHREADS), (int)CONFIG(PATCHVERSTRING),
        (int)CONFIG(SHOWGBABOOT),

        1 + (int)MULTICONFIG(DEFAULTEMU), 4 - (int)MULTICONFIG(BRIGHTNESS),
        splashPosStr, (unsigned int)cfg->splashDurationMsec,
        pinNumDigits, n3dsCpuStr,
        autobootModeStr,

        cfg->hbldr3dsxTitleId, rosalinaMenuComboStr,
        (int)cfg->ntpTzOffetMinutes,

        (int)cfg->topScreenFilter.cct, (int)cfg->bottomScreenFilter.cct,
        topScreenFilterGammaStr, bottomScreenFilterGammaStr,
        topScreenFilterContrastStr, bottomScreenFilterContrastStr,
        topScreenFilterBrightnessStr, bottomScreenFilterBrightnessStr,
        (int)cfg->topScreenFilter.invert, (int)cfg->bottomScreenFilter.invert,

        cfg->autobootTwlTitleId, (int)cfg->autobootCtrAppmemtype,

        forceAudioOutputStr,

        (int)CONFIG(PATCHUNITINFO), (int)CONFIG(DISABLEARM11EXCHANDLERS),
        (int)CONFIG(ENABLESAFEFIRMROSALINA)
    );

    return n < 0 ? 0 : (size_t)n;
}

Result LumaConfig_SaveSettings(void)
{
    char inibuf[0x2000];

    Result res;

    IFile file;
    u64 total;

    CfgData configData;

    u32 formatVersion;
    u32 config, multiConfig, bootConfig;
    u32 splashDurationMsec;

    u8 autobootCtrAppmemtype;
    u64 autobootTwlTitleId;

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

    svcGetSystemInfo(&out, 0x10000, 0x10);
    autobootTwlTitleId = (u64)out;
    svcGetSystemInfo(&out, 0x10000, 0x11);
    autobootCtrAppmemtype = (u8)out;

    svcGetSystemInfo(&out, 0x10000, 0x203);
    isSdMode = (bool)out;

    configData.formatVersionMajor = (u16)(formatVersion >> 16);
    configData.formatVersionMinor = (u16)formatVersion;
    configData.config = config;
    configData.multiConfig = multiConfig;
    configData.bootConfig = bootConfig;
    configData.splashDurationMsec = splashDurationMsec;
    configData.hbldr3dsxTitleId = Luma_SharedConfig->selected_hbldr_3dsx_tid;
    configData.rosalinaMenuCombo = menuCombo;
    configData.ntpTzOffetMinutes = (s16)lastNtpTzOffset;
    configData.topScreenFilter = topScreenFilter;
    configData.bottomScreenFilter = bottomScreenFilter;
    configData.autobootTwlTitleId = autobootTwlTitleId;
    configData.autobootCtrAppmemtype = autobootCtrAppmemtype;

    size_t n = LumaConfig_SaveLumaIniConfigToStr(inibuf, &configData);
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

    return res;
}
