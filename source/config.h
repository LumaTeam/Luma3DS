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
#define MULTICONFIG(a)   ((configData.config >> (a * 2 + 8)) & 3)
#define BOOTCONFIG(a, b) ((configData.config >> a) & b)

#define CONFIG_FILE         "config.bin"
#define CONFIG_VERSIONMAJOR 1
#define CONFIG_VERSIONMINOR 8

#define BOOTCFG_NAND         BOOTCONFIG(0, 7)
#define BOOTCFG_FIRM         BOOTCONFIG(3, 7)
#define BOOTCFG_A9LH         BOOTCONFIG(6, 1)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(7, 1)

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU,
    DEVOPTIONS
};

enum singleOptions
{
    AUTOBOOTSYS = 0,
    USESYSFIRM,
    LOADEXTFIRMSANDMODULES,
    USECUSTOMPATH,
    PATCHGAMES,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PATCHACCESS,
    HIDEPIN
};

typedef enum ConfigurationStatus
{
    DONT_CONFIGURE = 0,
    MODIFY_CONFIGURATION,
    CREATE_CONFIGURATION
} ConfigurationStatus;

bool readConfig(void);
void writeConfig(ConfigurationStatus needConfig, u32 configTemp);
void configMenu(bool isSdMode, bool oldPinStatus, u32 oldPinMode);
