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

#include <assert.h>
#include <strings.h>
#include "config.h"
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "emunand.h"
#include "buttons.h"
#include "pin.h"
#include "i2c.h"
#include "ini.h"

#include "config_template_ini.h" // note that it has an extra NUL byte inserted

#define MAKE_LUMA_VERSION_MCU(major, minor, build) (u16)(((major) & 0xFF) << 8 | ((minor) & 0x1F) << 5 | ((build) & 7))

CfgData configData;
ConfigurationStatus needConfig;
static CfgData oldConfig;

static CfgDataMcu configDataMcu;
static_assert(sizeof(CfgDataMcu) > 0, "wrong data size");

// INI parsing
// ===========================================================

static const char *singleOptionIniNamesBoot[] = {
    "autoboot_emunand",
    "use_emunand_firm_if_r_pressed",
    "enable_external_firm_and_modules",
    "enable_game_patching",
    "show_system_settings_string",
    "show_gba_boot_screen",
};

static const char *singleOptionIniNamesMisc[] = {
    "use_dev_unitinfo",
    "disable_arm11_exception_handlers",
    "enable_safe_firm_rosalina",
};

static const char *keyNames[] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y",
    "?", "?",
    "ZL", "ZR",
    "?", "?", "?", "?",
    "Touch",
    "?", "?", "?",
    "CStick Right", "CStick Left", "CStick Up", "CStick Down",
    "CPad Right", "CPad Left", "CPad Up", "CPad Down",
};

static int parseBoolOption(bool *out, const char *val)
{
    *out = false;
    if (strlen(val) != 1) {
        return -1;
    }

    if (val[0] == '0') {
        return 0;
    } else if (val[0] == '1') {
        *out = true;
        return 0;
    } else {
        return -1;
    }
}

static int parseDecIntOption(s64 *out, const char *val, s64 minval, s64 maxval)
{
    *out = 0;
    size_t numDigits = strlen(val);
    s64 res = 0;
    size_t i = 0;

    s64 sign = 1;
    if (numDigits >= 2) {
        if (val[0] == '+') {
            ++i;
        } else if (val[0] == '-') {
            sign = -1;
            ++i;
        }
    }

    for (; i < numDigits; i++) {
        u64 n = (u64)(val[i] - '0');
        if (n > 9) {
            return -1;
        }

        res = 10*res + n;
    }

    res *= sign;
    if (res <= maxval && res >= minval) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseHexIntOption(u64 *out, const char *val, u64 minval, u64 maxval)
{
    *out = 0;
    size_t numDigits = strlen(val);
    u64 res = 0;

    for (size_t i = 0; i < numDigits; i++) {
        char c = val[i];
        if ((u64)(c - '0') <= 9) {
            res = 16*res + (u64)(c - '0');
        } else if ((u64)(c - 'a') <= 5) {
            res = 16*res + (u64)(c - 'a' + 10);
        } else if ((u64)(c - 'A') <= 5) {
            res = 16*res + (u64)(c - 'A' + 10);
        } else {
            return -1;
        }
    }

    if (res <= maxval && res >= minval) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseKeyComboOption(u32 *out, const char *val)
{
    const char *startpos = val;
    const char *endpos;

    *out = 0;
    u32 keyCombo = 0;
    do {
        // Copy the button name (note that 16 chars is longer than any of the key names)
        char name[17];
        endpos = strchr(startpos, '+');
        size_t n = endpos == NULL ? 16 : endpos - startpos;
        n = n > 16 ? 16 : n;
        strncpy(name, startpos, n);
        name[n] = '\0';

        if (strcmp(name, "?") == 0) {
            // Lol no, bail out
            return -1;
        }

        bool found = false;
        for (size_t i = 0; i < sizeof(keyNames)/sizeof(keyNames[0]); i++) {
            if (strcasecmp(keyNames[i], name) == 0) {
                found = true;
                keyCombo |= 1u << i;
            }
        }

        if (!found) {
            return -1;
        }

        if (endpos != NULL) {
            startpos = endpos + 1;
        }
    } while(endpos != NULL && *startpos != '\0');

    if (*startpos == '\0') {
        // Trailing '+'
        return -1;
    } else {
        *out = keyCombo;
        return 0;
    }
}

static void menuComboToString(char *out, u32 combo)
{
    char *outOrig = out;
    out[0] = 0;
    for(int i = 31; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keyNames[i]);
            out += strlen(keyNames[i]);
            *out++ = '+';
        }
    }

    if (out != outOrig)
        out[-1] = 0;
}

static bool hasIniParseError = false;
static int iniParseErrorLine = 0;

#define CHECK_PARSE_OPTION(res) do { if((res) < 0) { hasIniParseError = true; iniParseErrorLine = lineno; return 0; } } while(false)

static int configIniHandler(void* user, const char* section, const char* name, const char* value, int lineno)
{
    CfgData *cfg = (CfgData *)user;
    if (strcmp(section, "meta") == 0) {
        if (strcmp(name, "config_version_major") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFF));
            cfg->formatVersionMajor = (u16)opt;
            return 1;
        } else if (strcmp(name, "config_version_minor") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFF));
            cfg->formatVersionMinor = (u16)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "boot") == 0) {
        // Simple options displayed on the Luma3DS boot screen
        for (size_t i = 0; i < sizeof(singleOptionIniNamesBoot)/sizeof(singleOptionIniNamesBoot[0]); i++) {
            if (strcmp(name, singleOptionIniNamesBoot[i]) == 0) {
                bool opt;
                CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
                cfg->config |= (u32)opt << i;
                return 1;
            }
        }

        // Multi-choice options displayed on the Luma3DS boot screen

        if (strcmp(name, "default_emunand_number") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1, 4));
            cfg->multiConfig |= (opt - 1) << (2 * (u32)DEFAULTEMU);
            return 1;
        } else if (strcmp(name, "brightness_level") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1, 4));
            cfg->multiConfig |= (4 - opt) << (2 * (u32)BRIGHTNESS);
            return 1;
        } else if (strcmp(name, "splash_position") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)SPLASH);
                return 1;
            } else if (strcasecmp(value, "before payloads") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)SPLASH);
                return 1;
            } else if (strcasecmp(value, "after payloads") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)SPLASH);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "splash_duration_ms") == 0) {
            // Not displayed in the menu anymore, but more configurable
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFFFFFFu));
            cfg->splashDurationMsec = (u32)opt;
            return 1;
        }
        else if (strcmp(name, "pin_lock_num_digits") == 0) {
            s64 opt;
            u32 encodedOpt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 8));
            // Only allow for 0 (off), 4, 6 or 8 'digits'
            switch (opt) {
                case 0: encodedOpt = 0; break;
                case 4: encodedOpt = 1; break;
                case 6: encodedOpt = 2; break;
                case 8: encodedOpt = 3; break;
                default: {
                    CHECK_PARSE_OPTION(-1);
                }
            }
            cfg->multiConfig |= encodedOpt << (2 * (u32)PIN);
            return 1;
        } else if (strcmp(name, "app_launch_new_3ds_cpu") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "clock") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "l2") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "clock+l2") == 0) {
                cfg->multiConfig |= 3 << (2 * (u32)NEWCPU);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        }
        CHECK_PARSE_OPTION(-1);
    } else if (strcmp(section, "rosalina") == 0) {
        // Rosalina options
        if (strcmp(name, "hbldr_3dsx_titleid") == 0) {
            u64 opt;
            CHECK_PARSE_OPTION(parseHexIntOption(&opt, value, 0, 0xFFFFFFFFFFFFFFFFull));
            cfg->hbldr3dsxTitleId = opt;
            return 1;
        } else if (strcmp(name, "rosalina_menu_combo") == 0) {
            u32 opt;
            CHECK_PARSE_OPTION(parseKeyComboOption(&opt, value));
            cfg->rosalinaMenuCombo = opt;
            return 1;
        } else if (strcmp(name, "plugin_loader_enabled") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->pluginLoaderFlags = opt ? cfg->pluginLoaderFlags | 1 : cfg->pluginLoaderFlags & ~1;
            return 1;
        } else if (strcmp(name, "screen_filters_cct") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1000, 25100));
            cfg->screenFiltersCct = (u32)opt;
            return 1;
        } else if (strcmp(name, "ntp_tz_offset_min") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, -779, 899));
            cfg->ntpTzOffetMinutes = (s16)opt;
            return 1;
        }
        else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "misc") == 0) {
        for (size_t i = 0; i < sizeof(singleOptionIniNamesMisc)/sizeof(singleOptionIniNamesMisc[0]); i++) {
            if (strcmp(name, singleOptionIniNamesMisc[i]) == 0) {
                bool opt;
                CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
                cfg->config |= (u32)opt << (i + (u32)PATCHUNITINFO);
                return 1;
            }
        }
        CHECK_PARSE_OPTION(-1);
    } else {
        CHECK_PARSE_OPTION(-1);
    }
}

static size_t saveLumaIniConfigToStr(char *out)
{
    const CfgData *cfg = &configData;

    char lumaVerStr[64];
    char lumaRevSuffixStr[16];
    char rosalinaMenuComboStr[128];

    const char *splashPosStr;
    const char *n3dsCpuStr;

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

    if (VERSION_BUILD != 0) {
        sprintf(lumaVerStr, "Luma3DS v%d.%d.%d", (int)VERSION_MAJOR, (int)VERSION_MINOR, (int)VERSION_BUILD);
    } else {
        sprintf(lumaVerStr, "Luma3DS v%d.%d", (int)VERSION_MAJOR, (int)VERSION_MINOR);
    }

    if (ISRELEASE) {
        strcpy(lumaRevSuffixStr, "");
    } else {
        sprintf(lumaRevSuffixStr, "-%08lx", (u32)COMMIT_HASH);
    }

    menuComboToString(rosalinaMenuComboStr, cfg->rosalinaMenuCombo);

    static const int pinOptionToDigits[] = { 0, 4, 6, 8 };
    int pinNumDigits = pinOptionToDigits[MULTICONFIG(PIN)];

    int n = sprintf(
        out, (const char *)config_template_ini,
        lumaVerStr, lumaRevSuffixStr,

        (int)CONFIG_VERSIONMAJOR, (int)CONFIG_VERSIONMINOR,
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

static char tmpIniBuffer[0x2000];

static bool readLumaIniConfig(void)
{
    u32 rd = fileRead(tmpIniBuffer, "config.ini", sizeof(tmpIniBuffer) - 1);
    if (rd == 0) return false;

    tmpIniBuffer[rd] = '\0';

    return ini_parse_string(tmpIniBuffer, &configIniHandler, &configData) >= 0 && !hasIniParseError;
}

static bool writeLumaIniConfig(void)
{
    size_t n = saveLumaIniConfigToStr(tmpIniBuffer);
    return n != 0 && fileWrite(tmpIniBuffer, "config.ini", n);
}

// ===========================================================

static void writeConfigMcu(void)
{
    u8 data[sizeof(CfgDataMcu)];

    // Set Luma version
    configDataMcu.lumaVersion = MAKE_LUMA_VERSION_MCU(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    // Set bootconfig from CfgData
    configDataMcu.bootCfg = configData.bootConfig;

    memcpy(data, &configDataMcu, sizeof(CfgDataMcu));

    // Fix checksum
    u8 checksum = 0;
    for (u32 i = 0; i < sizeof(CfgDataMcu) - 1; i++)
        checksum += data[i];
    checksum = ~checksum;
    data[sizeof(CfgDataMcu) - 1] = checksum;
    configDataMcu.checksum = checksum;

    I2C_writeReg(I2C_DEV_MCU, 0x60, 200 - sizeof(CfgDataMcu));
    I2C_writeRegBuf(I2C_DEV_MCU, 0x61, data, sizeof(CfgDataMcu));
}

static bool readConfigMcu(void)
{
    u8 data[sizeof(CfgDataMcu)];
    u16 curVer = MAKE_LUMA_VERSION_MCU(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    // Select free reg id, then access the data regs
    I2C_writeReg(I2C_DEV_MCU, 0x60, 200 - sizeof(CfgDataMcu));
    I2C_readRegBuf(I2C_DEV_MCU, 0x61, data, sizeof(CfgDataMcu));
    memcpy(&configDataMcu, data, sizeof(CfgDataMcu));

    u8 checksum = 0;
    for (u32 i = 0; i < sizeof(CfgDataMcu) - 1; i++)
        checksum += data[i];
    checksum = ~checksum;

    if (checksum != configDataMcu.checksum || configDataMcu.lumaVersion < MAKE_LUMA_VERSION_MCU(10, 3, 0))
    {
        // Invalid data stored in MCU...
        memset(&configDataMcu, 0, sizeof(CfgDataMcu));
        configData.bootConfig = 0;
        // Perform upgrade process (ignoring failures)
        doLumaUpgradeProcess();
        writeConfigMcu();

        return false;
    }

    if (configDataMcu.lumaVersion < curVer)
    {
        // Perform upgrade process (ignoring failures)
        doLumaUpgradeProcess();
        writeConfigMcu();
    }

    return true;
}

bool readConfig(void)
{
    bool retMcu, ret;

    retMcu = readConfigMcu();
    ret = readLumaIniConfig();
    if(!retMcu || !ret ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        memset(&configData, 0, sizeof(CfgData));
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;
        configData.config |= 1u << PATCHVERSTRING;
        configData.splashDurationMsec = 3000;
        configData.hbldr3dsxTitleId = 0x000400000D921E00ull;
        configData.rosalinaMenuCombo = 1u << 9 | 1u << 7 | 1u << 2; // L+Start+Select
        configData.screenFiltersCct = 6500; // default temp, no-op
        ret = false;
    }
    else
        ret = true;

    configData.bootConfig = configDataMcu.bootCfg;
    oldConfig = configData;

    return ret;
}

void writeConfig(bool isConfigOptions)
{
    bool updateMcu, updateIni;

    if (needConfig == CREATE_CONFIGURATION)
    {
        updateMcu = !isConfigOptions; // We've already committed it once (if it wasn't initialized)
        updateIni = isConfigOptions;
        needConfig = MODIFY_CONFIGURATION;
    }
    else
    {
        updateMcu = !isConfigOptions && configData.bootConfig != oldConfig.bootConfig;
        updateIni = isConfigOptions && (configData.config != oldConfig.config || configData.multiConfig != oldConfig.multiConfig);
    }

    if (updateMcu)
        writeConfigMcu();

    if(updateIni && !writeLumaIniConfig())
        error("Error writing the configuration file");
}

void configMenu(bool oldPinStatus, u32 oldPinMode)
{
    static const char *multiOptionsText[]  = { "Default EmuNAND: 1( ) 2( ) 3( ) 4( )",
                                               "Screen brightness: 4( ) 3( ) 2( ) 1( )",
                                               "Splash: Off( ) Before( ) After( ) payloads",
                                               "PIN lock: Off( ) 4( ) 6( ) 8( ) digits",
                                               "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )",
                                             };

    static const char *singleOptionsText[] = { "( ) Autoboot EmuNAND",
                                               "( ) Use EmuNAND FIRM if booting with R",
                                               "( ) Enable loading external FIRMs and modules",
                                               "( ) Enable game patching",
                                               "( ) Show NAND or user string in System Settings",
                                               "( ) Show GBA boot screen in patched AGB_FIRM",
                                             };

    static const char *optionsDescription[]  = { "Select the default EmuNAND.\n\n"
                                                 "It will be booted when no\n"
                                                 "directional pad buttons are pressed.",

                                                 "Select the screen brightness.",

                                                 "Enable splash screen support.\n\n"
                                                 "\t* 'Before payloads' displays it\n"
                                                 "before booting payloads\n"
                                                 "(intended for splashes that display\n"
                                                 "button hints).\n\n"
                                                 "\t* 'After payloads' displays it\n"
                                                 "afterwards.\n\n"
                                                 "Edit the duration in config.ini (3s\n"
                                                 "default).",

                                                 "Activate a PIN lock.\n\n"
                                                 "The PIN will be asked each time\n"
                                                 "Luma3DS boots.\n\n"
                                                 "4, 6 or 8 digits can be selected.\n\n"
                                                 "The ABXY buttons and the directional\n"
                                                 "pad buttons can be used as keys.\n\n"
                                                 "A message can also be displayed\n"
                                                 "(refer to the wiki for instructions).",

                                                 "Select the New 3DS CPU mode.\n\n"
                                                 "This won't apply to\n"
                                                 "New 3DS exclusive/enhanced games.\n\n"
                                                 "'Clock+L2' can cause issues with some\n"
                                                 "games.",

                                                 "If enabled, an EmuNAND\n"
                                                 "will be launched on boot.\n\n"
                                                 "Otherwise, SysNAND will.\n\n"
                                                 "Hold L on boot to switch NAND.\n\n"
                                                 "To use a different EmuNAND from the\n"
                                                 "default, hold a directional pad button\n"
                                                 "(Up/Right/Down/Left equal EmuNANDs\n"
                                                 "1/2/3/4).",

                                                 "If enabled, when holding R on boot\n"
                                                 "SysNAND will be booted with an\n"
                                                 "EmuNAND FIRM.\n\n"
                                                 "Otherwise, an EmuNAND will be booted\n"
                                                 "with the SysNAND FIRM.\n\n"
                                                 "To use a different EmuNAND from the\n"
                                                 "default, hold a directional pad button\n"
                                                 "(Up/Right/Down/Left equal EmuNANDs\n"
                                                 "1/2/3/4), also add A if you have\n"
                                                 "a matching payload.",

                                                 "Enable loading external FIRMs and\n"
                                                 "system modules.\n\n"
                                                 "This isn't needed in most cases.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable overriding the region and\n"
                                                 "language configuration and the usage\n"
                                                 "of patched code binaries, exHeaders,\n"
                                                 "IPS code patches and LayeredFS\n"
                                                 "for specific games.\n\n"
                                                 "Also makes certain DLCs\n"
                                                 "for out-of-region games work.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable showing the current NAND/FIRM:\n\n"
                                                 "\t* Sys  = SysNAND\n"
                                                 "\t* Emu  = EmuNAND 1\n"
                                                 "\t* EmuX = EmuNAND X\n"
                                                 "\t* SysE = SysNAND with EmuNAND 1 FIRM\n"
                                                 "\t* SyEX = SysNAND with EmuNAND X FIRM\n"
                                                 "\t* EmuS = EmuNAND 1 with SysNAND FIRM\n"
                                                 "\t* EmXS = EmuNAND X with SysNAND FIRM\n\n"
                                                 "or a user-defined custom string in\n"
                                                 "System Settings.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable showing the GBA boot screen\n"
                                                 "when booting GBA games.",
                                               };

    FirmwareSource nandType = FIRMWARE_SYSNAND;
    if(isSdMode)
    {
        nandType = FIRMWARE_EMUNAND;
        locateEmuNand(&nandType);
    }

    struct multiOption {
        u32 posXs[4];
        u32 posY;
        u32 enabled;
        bool visible;
    } multiOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = ISN3DS },
    };

    struct singleOption {
        u32 posY;
        bool enabled;
        bool visible;
    } singleOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptions) / sizeof(struct singleOption),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption = 0,
        singleSelected = 0;
    bool isMultiOption = false;

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        //Detect the positions where the "x" should go
        u32 optionNum = 0;
        for(u32 j = 0; optionNum < 4 && j < strlen(multiOptionsText[i]); j++)
            if(multiOptionsText[i][j] == '(') multiOptions[i].posXs[optionNum++] = j + 1;
        while(optionNum < 4) multiOptions[i].posXs[optionNum++] = 0;

        multiOptions[i].enabled = MULTICONFIG(i);
    }
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    initScreens();

    static const char *bootTypes[] = { "B9S",
                                       "B9S (ntrboot)",
                                       "FIRM0",
                                       "FIRM1" };

    drawString(true, 10, 10, COLOR_TITLE, CONFIG_TITLE);
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Press A to select, START to save");
    drawFormattedString(false, 10, SCREEN_HEIGHT - 2 * SPACING_Y, COLOR_YELLOW, "Booted from %s via %s", isSdMode ? "SD" : "CTRNAND", bootTypes[(u32)bootType]);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 10 + 2 * SPACING_Y;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!multiOptions[i].visible) continue;

        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, multiOptions[i].posY, COLOR_WHITE, multiOptionsText[i]);
        drawCharacter(true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE, selected);
    }

    endPos += SPACING_Y / 2;

    //Display all the normal options in white except for the first one
    for(u32 i = 0, color = COLOR_RED; i < singleOptionsAmount; i++)
    {
        if(!singleOptions[i].visible) continue;

        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, singleOptions[i].posY, color, singleOptionsText[i]);
        if(singleOptions[i].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[i].posY, color, selected);

        if(color == COLOR_RED)
        {
            singleSelected = i;
            selectedOption = i + multiOptionsAmount;
            color = COLOR_WHITE;
        }
    }

    drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);

    //Boring configuration menu
    while(true)
    {
        u32 pressed;
        do
        {
            pressed = waitInput(true) & MENU_BUTTONS;
        }
        while(!pressed);

        if(pressed == BUTTON_START) break;

        if(pressed != BUTTON_A)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            while(true)
            {
                switch(pressed)
                {
                    case BUTTON_UP:
                        selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                        break;
                    case BUTTON_DOWN:
                        selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                        break;
                    case BUTTON_LEFT:
                        pressed = BUTTON_DOWN;
                        selectedOption = 0;
                        break;
                    case BUTTON_RIGHT:
                        pressed = BUTTON_UP;
                        selectedOption = totalIndexes;
                        break;
                    default:
                        break;
                }

                if(selectedOption < multiOptionsAmount)
                {
                    if(!multiOptions[selectedOption].visible) continue;

                    isMultiOption = true;
                    break;
                }
                else
                {
                    singleSelected = selectedOption - multiOptionsAmount;

                    if(!singleOptions[singleSelected].visible) continue;

                    isMultiOption = false;
                    break;
                }
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(true, 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE, multiOptionsText[oldSelectedOption]);
                drawCharacter(true, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE, selected);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(true, 10, singleOptions[singleOldSelected].posY, COLOR_WHITE, singleOptionsText[singleOldSelected]);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE, selected);
            }

            if(isMultiOption) drawString(true, 10, multiOptions[selectedOption].posY, COLOR_RED, multiOptionsText[selectedOption]);
            else drawString(true, 10, singleOptions[singleSelected].posY, COLOR_RED, singleOptionsText[singleSelected]);

            drawString(false, 10, 10, COLOR_BLACK, optionsDescription[oldSelectedOption]);
            drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);
        }
        else
        {
            //The selected option's status changed, print the 'x's accordingly
            if(isMultiOption)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(true, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK, selected);
                multiOptions[selectedOption].enabled = (oldEnabled == 3 || !multiOptions[selectedOption].posXs[oldEnabled + 1]) ? 0 : oldEnabled + 1;

                if(selectedOption == BRIGHTNESS) updateBrightness(multiOptions[BRIGHTNESS].enabled);
            }
            else
            {
                bool oldEnabled = singleOptions[singleSelected].enabled;
                singleOptions[singleSelected].enabled = !oldEnabled;
                if(oldEnabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_BLACK, selected);
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(isMultiOption) drawCharacter(true, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED, selected);
        else if(singleOptions[singleSelected].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED, selected);
    }

    //Parse and write the new configuration
    configData.multiConfig = 0;
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.multiConfig |= multiOptions[i].enabled << (i * 2);

    configData.config &= ~((1 << (u32)NUMCONFIGURABLE) - 1);
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << i;

    writeConfig(true);

    u32 newPinMode = MULTICONFIG(PIN);

    if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
    else if(oldPinStatus)
    {
        if(!fileDelete(PIN_FILE))
            error("Unable to delete PIN file");
    }

    while(HID_PAD & PIN_BUTTONS);
    wait(2000ULL);
}
