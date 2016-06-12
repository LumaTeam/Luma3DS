/*
*   types.h
*/

#pragma once

#include <stdint.h>
#include <stdlib.h>

//Common data types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;

//Used by multiple files:
typedef enum FirmwareSource
{
    FIRMWARE_SYSNAND = 0,
    FIRMWARE_EMUNAND = 1,
    FIRMWARE_EMUNAND2 = 2
} FirmwareSource;

typedef enum FirmwareType
{
    NATIVE_FIRM = 0,
    TWL_FIRM = 1,
    AGB_FIRM = 2,
    SAFE_FIRM = 3
} FirmwareType;