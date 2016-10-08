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
*   Crypto libs from http://github.com/b1l1s/ctr
*   kernel9Loader code originally adapted from https://github.com/Reisyukaku/ReiNand/blob/228c378255ba693133dec6f3368e14d386f2cde7/source/crypto.c#L233
*   decryptNusFirm code adapted from https://github.com/mid-kid/CakesForeveryWan/blob/master/source/firm.c
*   3ds type structs adapted from 3DBrew and https://github.com/mid-kid/CakesForeveryWan/blob/master/source/headers.h
*/

#pragma once

#include "types.h"

/**************************AES****************************/
#define REG_AESCNT          ((vu32 *)0x10009000)
#define REG_AESBLKCNT       ((vu32 *)0x10009004)
#define REG_AESWRFIFO       ((vu32 *)0x10009008)
#define REG_AESRDFIFO       ((vu32 *)0x1000900C)
#define REG_AESKEYSEL       ((vu8 *)0x10009010)
#define REG_AESKEYCNT       ((vu8 *)0x10009011)
#define REG_AESCTR          ((vu32 *)0x10009020)

#define REG_AESKEYFIFO      ((vu32 *)0x10009100)
#define REG_AESKEYXFIFO     ((vu32 *)0x10009104)
#define REG_AESKEYYFIFO     ((vu32 *)0x10009108)

#define AES_CCM_DECRYPT_MODE    (0u << 27)
#define AES_CCM_ENCRYPT_MODE    (1u << 27)
#define AES_CTR_MODE            (2u << 27)
#define AES_CTR_MODE            (2u << 27)
#define AES_CBC_DECRYPT_MODE    (4u << 27)
#define AES_CBC_ENCRYPT_MODE    (5u << 27)
#define AES_ECB_DECRYPT_MODE    (6u << 27)
#define AES_ECB_ENCRYPT_MODE    (7u << 27)
#define AES_ALL_MODES           (7u << 27)

#define AES_CNT_START           0x80000000
#define AES_CNT_INPUT_ORDER     0x02000000
#define AES_CNT_OUTPUT_ORDER    0x01000000
#define AES_CNT_INPUT_ENDIAN    0x00800000
#define AES_CNT_OUTPUT_ENDIAN   0x00400000
#define AES_CNT_FLUSH_READ      0x00000800
#define AES_CNT_FLUSH_WRITE     0x00000400

#define AES_INPUT_BE        (AES_CNT_INPUT_ENDIAN)
#define AES_INPUT_LE        0
#define AES_INPUT_NORMAL    (AES_CNT_INPUT_ORDER)
#define AES_INPUT_REVERSED  0

#define AES_BLOCK_SIZE      0x10

#define AES_KEYCNT_WRITE    (1 << 0x7)
#define AES_KEYNORMAL       0
#define AES_KEYX        1
#define AES_KEYY        2

/**************************SHA****************************/
#define REG_SHA_CNT         ((vu32 *)0x1000A000)
#define REG_SHA_BLKCNT      ((vu32 *)0x1000A004)
#define REG_SHA_HASH        ((vu32 *)0x1000A040)
#define REG_SHA_INFIFO      ((vu32 *)0x1000A080)

#define SHA_CNT_STATE           0x00000003
#define SHA_CNT_UNK2            0x00000004
#define SHA_CNT_OUTPUT_ENDIAN   0x00000008
#define SHA_CNT_MODE            0x00000030
#define SHA_CNT_ENABLE          0x00010000
#define SHA_CNT_ACTIVE          0x00020000

#define SHA_HASH_READY      0x00000000
#define SHA_NORMAL_ROUND    0x00000001
#define SHA_FINAL_ROUND     0x00000002

#define SHA_OUTPUT_BE       SHA_CNT_OUTPUT_ENDIAN
#define SHA_OUTPUT_LE       0

#define SHA_256_MODE        0
#define SHA_224_MODE        0x00000010
#define SHA_1_MODE          0x00000020

#define SHA_256_HASH_SIZE   (256 / 8)
#define SHA_224_HASH_SIZE   (224 / 8)
#define SHA_1_HASH_SIZE     (160 / 8)

typedef struct Ncch {
    uint8_t sig[0x100]; //RSA-2048 signature of the NCCH header, using SHA-256
    char magic[4]; //NCCH
    uint32_t contentSize; //Media unit
    uint8_t partitionId[8];
    uint8_t makerCode[2];
    uint16_t version;
    uint8_t reserved1[4];
    uint8_t programID[8];
    uint8_t reserved2[0x10];
    uint8_t logoHash[0x20]; //Logo Region SHA-256 hash
    char productCode[0x10];
    uint8_t exHeaderHash[0x20]; //Extended header SHA-256 hash
    uint32_t exHeaderSize; //Extended header size
    uint32_t reserved3;
    uint8_t flags[8];
    uint32_t plainOffset; //Media unit
    uint32_t plainSize; //Media unit
    uint32_t logoOffset; //Media unit
    uint32_t logoSize; //Media unit
    uint32_t exeFsOffset; //Media unit
    uint32_t exeFsSize; //Media unit
    uint32_t exeFsHashSize; //Media unit
    uint32_t reserved4;
    uint32_t romFsOffset; //Media unit
    uint32_t romFsSize; //Media unit
    uint32_t romFsHashSize; //Media unit
    uint32_t reserved5;
    uint8_t exeFsHash[0x20]; //ExeFS superblock SHA-256 hash
    uint8_t romFsHash[0x20]; //RomFS superblock SHA-256 hash
} Ncch;

typedef struct Ticket
{
    char sigIssuer[0x40];
    uint8_t eccPubKey[0x3C];
    uint8_t version;
    uint8_t caCrlVersion;
    uint8_t signerCrlVersion;
    uint8_t titleKey[0x10];
    uint8_t reserved1;
    uint8_t ticketId[8];
    uint8_t consoleId[4];
    uint8_t titleId[8];
    uint8_t reserved2[2];
    uint16_t ticketTitleVersion;
    uint8_t reserved3[8];
    uint8_t licenseType;
    uint8_t ticketCommonKeyYIndex; //Ticket common keyY index, usually 0x1 for retail system titles.
    uint8_t reserved4[0x2A];
    uint8_t unk[4]; //eShop Account ID?
    uint8_t reserved5;
    uint8_t audit;
    uint8_t reserved6[0x42];
    uint8_t limits[0x40];
    uint8_t contentIndex[0xAC];
} Ticket;

typedef struct Arm9Bin {
    uint8_t keyX[0x10];
    uint8_t keyY[0x10];
    uint8_t ctr[0x10];
    uint8_t size[8];
    uint8_t reserved[8];
    uint8_t ctlBlock[0x10];
    char magic[4];
    uint8_t reserved2[0xC];
    uint8_t slot0x16keyX[0x10];
} Arm9Bin;

extern u32 emuOffset;
extern bool isN3DS, isDevUnit, isA9lh;
extern FirmwareSource firmSource;

void ctrNandInit(void);
int ctrNandRead(u32 sector, u32 sectorCount, u8 *outbuf);
int ctrNandWrite(u32 sector, u32 sectorCount, u8 *inbuf);
void set6x7xKeys(void);
void decryptExeFs(Ncch *ncch);
void decryptNusFirm(const Ticket *ticket, Ncch *ncch, u32 ncchSize);
void kernel9Loader(Arm9Bin *arm9Section);
void computePinHash(u8 *outbuf, const u8 *inbuf);
void restoreShaHashBackup(void);