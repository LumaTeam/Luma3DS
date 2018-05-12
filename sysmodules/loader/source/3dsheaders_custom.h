/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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

/*
*   Adapted from 3DBrew and https://github.com/mid-kid/CakesForeveryWan/blob/master/source/headers.h
*/

#pragma once
#include "exheader.h"

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
    exheader_header exHeader;
} Cxi;

typedef struct __attribute__((packed))
{
    struct __attribute__((packed))
    {
        char name[8];
        u32 offset;
        u32 size;
    } headers[10];
    char reserved[0x20];
    char fileHashes[0x140]; // Ignored for now
} ExeFS;
