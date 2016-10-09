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

/*
*   Adapted from 3DBrew and https://github.com/mid-kid/CakesForeveryWan/blob/master/source/headers.h
*/

typedef struct __attribute__((packed))
{
    u32 address;
    u32 phyRegionSize;
    u32 size;
} CodeSetInfo;

typedef struct __attribute__((packed))
{
    u32 saveDataSize[2];
    u32 jumpID[2];
    u8 reserved[0x30];
} SystemInfo;

typedef struct __attribute__((packed))
{
    char appTitle[8];
    u8 reserved1[5];
    u8 flag;
    u8 remasterVersion[2];
    CodeSetInfo textCodeSet;
    u32 stackSize;
    CodeSetInfo roCodeSet;
    u8 reserved2[4];
    CodeSetInfo dataCodeSet;
    u32 bssSize;
    char depends[0x180];
    SystemInfo systemInfo;
} SystemControlInfo;

typedef struct __attribute__((packed))
{
    SystemControlInfo systemControlInfo;
    u8 aci[0x200];
    u8 accessDescSig[0x100];
    u8 ncchPubKey[0x100];
    u8 aciLim[0x200];
} ExHeader;

typedef struct __attribute__((packed))
{
    u8 sig[0x100]; //RSA-2048 signature of the NCCH header, using SHA-256
    char magic[4]; //NCCH
    u32 contentSize; //Media unit
    u8 partitionId[8];
    u8 makerCode[2];
    u16 version;
    u8 reserved1[4];
    u8 programID[8];
    u8 reserved2[0x10];
    u8 logoHash[0x20]; //Logo Region SHA-256 hash
    char productCode[0x10];
    u8 exHeaderHash[0x20]; //Extended header SHA-256 hash
    u32 exHeaderSize; //Extended header size
    u32 reserved3;
    u8 flags[8];
    u32 plainOffset; //Media unit
    u32 plainSize; //Media unit
    u32 logoOffset; //Media unit
    u32 logoSize; //Media unit
    u32 exeFsOffset; //Media unit
    u32 exeFsSize; //Media unit
    u32 exeFsHashSize; //Media unit
    u32 reserved4;
    u32 romFsOffset; //Media unit
    u32 romFsSize; //Media unit
    u32 romFsHashSize; //Media unit
    u32 reserved5;
    u8 exeFsHash[0x20]; //ExeFS superblock SHA-256 hash
    u8 romFsHash[0x20]; //RomFS superblock SHA-256 hash
} Ncch;

typedef struct __attribute__((packed))
{
    Ncch ncch;
    ExHeader exHeader;
} Cxi;

typedef struct __attribute__((packed))
{
    char sigIssuer[0x40];
    u8 eccPubKey[0x3C];
    u8 version;
    u8 caCrlVersion;
    u8 signerCrlVersion;
    u8 titleKey[0x10];
    u8 reserved1;
    u8 ticketId[8];
    u8 consoleId[4];
    u8 titleId[8];
    u8 reserved2[2];
    u16 ticketTitleVersion;
    u8 reserved3[8];
    u8 licenseType;
    u8 ticketCommonKeyYIndex; //Ticket common keyY index, usually 0x1 for retail system titles.
    u8 reserved4[0x2A];
    u8 unk[4]; //eShop Account ID?
    u8 reserved5;
    u8 audit;
    u8 reserved6[0x42];
    u8 limits[0x40];
    u8 contentIndex[0xAC];
} Ticket;

typedef struct __attribute__((packed))
{
    u32 offset;
    u8 *address;
    u32 size;
    u32 procType;
    u8 hash[0x20];
} FirmSection;

typedef struct __attribute__((packed))
{
    u32 magic;
    u32 reserved1;
    u8 *arm11Entry;
    u8 *arm9Entry;
    u8 reserved2[0x30];
    FirmSection section[4];
} Firm;

typedef struct __attribute__((packed))
{
    u8 keyX[0x10];
    u8 keyY[0x10];
    u8 ctr[0x10];
    char size[8];
    u8 reserved[8];
    u8 ctlBlock[0x10];
    char magic[4];
    u8 reserved2[0xC];
    u8 slot0x16keyX[0x10];
} Arm9Bin;