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

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

//Common data types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

#include "3dsheaders.h"

#define BRAHMA_ARM11_ENTRY 0x1FFFFFF8

#define CFG_BOOTENV    (*(vu32 *)0x10010000)
#define CFG_UNITINFO   (*(vu8  *)0x10010010)
#define PDN_MPCORE_CFG (*(vu32 *)0x10140FFC)
#define PDN_SPI_CNT    (*(vu32 *)0x101401C0)

#define ISN3DS       (PDN_MPCORE_CFG == 7)
#define ISDEVUNIT    (CFG_UNITINFO != 0)
#define ISA9LH       (!PDN_SPI_CNT)
#define ISFIRMLAUNCH (launchedFirmTidLow[5] != 0)

typedef struct __attribute__((packed))
{
    char magic[4];
    u16 formatVersionMajor, formatVersionMinor;

    u32 config;
} CfgData;

typedef struct __attribute__((packed))
{
    char magic[4];
    u16 formatVersionMajor, formatVersionMinor;

    u8 lengthHash[32];
    u8 hash[32];
} PinData;

typedef struct __attribute__((packed))
{
    u32 magic[2];
    u16 versionMinor, versionMajor;

    u16 processor, core;
    u32 type;

    u32 totalSize;
    u32 registerDumpSize;
    u32 codeDumpSize;
    u32 stackDumpSize;
    u32 additionalDataSize;
} ExceptionDumpHeader;

typedef enum FirmwareSource
{
    FIRMWARE_SYSNAND = 0,
    FIRMWARE_EMUNAND,
    FIRMWARE_EMUNAND2,
    FIRMWARE_EMUNAND3,
    FIRMWARE_EMUNAND4
} FirmwareSource;

typedef enum FirmwareType
{
    NATIVE_FIRM = 0,
    TWL_FIRM,
    AGB_FIRM,
    SAFE_FIRM,
    SYSUPDATER_FIRM,
    NATIVE_FIRM1X2X
} FirmwareType;

extern u16 launchedFirmTidLow[8]; //Defined in start.s