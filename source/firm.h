/*
*   firm.h
*/

#pragma once

#include "types.h"

#define PDN_MPCORE_CFG (*(vu32 *)0x10140FFC)
#define PDN_SPI_CNT    (*(vu32 *)0x101401C0)
#define CFG_BOOTENV    (*(vu32 *)0x10010000)

//FIRM Header layout
typedef struct firmSectionHeader {
    u32 offset;
    u8 *address;
    u32 size;
    u32 procType;
    u8 hash[0x20];
} firmSectionHeader;

typedef struct firmHeader {
    u32 magic;
    u32 reserved1;
    u8 *arm11Entry;
    u8 *arm9Entry;
    u8 reserved2[0x30];
    firmSectionHeader section[4];
} firmHeader;

typedef enum ConfigurationStatus
{
    DONT_CONFIGURE = 0,
    MODIFY_CONFIGURATION = 1,
    CREATE_CONFIGURATION = 2
} ConfigurationStatus;

typedef enum A9LHMode
{
    NO_A9LH = 0,
    A9LH_WITH_NFIRM_FIRMPROT = 1,
    A9LH_WITH_SFIRM_FIRMPROT = 2
} A9LHMode;

static inline void loadFirm(FirmwareType firmType, bool externalFirm);
static inline void patchNativeFirm(FirmwareSource nandType, u32 emuHeader, A9LHMode a9lhMode);
static inline void patchLegacyFirm(FirmwareType firmType);
static inline void patchSafeFirm(void);
static inline void copySection0AndInjectLoader(void);
static inline void launchFirm(FirmwareType firmType, bool isFirmlaunch);