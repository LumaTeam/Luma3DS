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

typedef struct patchData {
    u32 offset[2];
    union {
        u8 type0[8];
        u16 type1;
    } patch;
    u32 type;
} patchData;

static inline void loadFirm(u32 firmType, u32 externalFirm);
static inline void patchNativeFirm(u32 nandType, u32 emuHeader, u32 a9lhMode);
static inline void patchEmuNAND(u8 *arm9Section, u8 *proc9Offset, u32 emuHeader);
static inline void patchReboots(u8 *arm9Section, u8 *proc9Offset);
static inline void reimplementSvcBackdoor(u8 *arm11Section1);
static inline void copySection0AndInjectLoader(void);
static inline void patchLegacyFirm(u32 firmType);
static inline void patchSafeFirm(void);
static void patchFirmWrites(u8 *arm9Section, u32 mode);
static inline void launchFirm(u32 firstSectionToCopy, u32 bootType);