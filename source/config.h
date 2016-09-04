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

#pragma once

#include "types.h"

#define CONFIG(a)        (((configData.config >> (a + 20)) & 1) != 0)
#define MULTICONFIG(a)   ((configData.config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((configData.config >> a) & b)

#define CONFIG_PATH         "/luma/config.bin"
#define CONFIG_VERSIONMAJOR 1
#define CONFIG_VERSIONMINOR 2

#define BOOTCFG_NAND         BOOTCONFIG(0, 3)
#define BOOTCFG_FIRM         BOOTCONFIG(2, 1)
#define BOOTCFG_A9LH         BOOTCONFIG(3, 1)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(4, 1)
#define BOOTCFG_SAFEMODE     BOOTCONFIG(5, 1)
#define CONFIG_BRIGHTNESS    MULTICONFIG(0)
#define CONFIG_PIN           MULTICONFIG(1)
#define CONFIG_DEVOPTIONS    MULTICONFIG(3)
#define CONFIG_AUTOBOOTSYS   CONFIG(0)
#define CONFIG_USESYSFIRM    CONFIG(1)
#define CONFIG_USESECONDEMU  CONFIG(2)
#define CONFIG_SHOWGBABOOT   CONFIG(5)
#define CONFIG_PAYLOADSPLASH CONFIG(6)
#define CONFIG_PATCHACCESS   CONFIG(7)

typedef struct __attribute__((packed))
{
    char magic[4];
    u16 formatVersionMajor, formatVersionMinor;

    u32 config;
} cfgData;

typedef enum ConfigurationStatus
{
    DONT_CONFIGURE = 0,
    MODIFY_CONFIGURATION = 1,
    CREATE_CONFIGURATION = 2
} ConfigurationStatus;

extern cfgData configData;

bool readConfig(void);
void writeConfig(ConfigurationStatus needConfig, u32 configTemp);
void configMenu(bool oldPinStatus);