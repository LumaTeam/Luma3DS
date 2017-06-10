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
*   Crypto libs from http://github.com/b1l1s/ctr
*   kernel9Loader code originally adapted from https://github.com/Reisyukaku/ReiNand/blob/228c378255ba693133dec6f3368e14d386f2cde7/source/crypto.c#L233
*   decryptNusFirm code adapted from https://github.com/mid-kid/CakesForeveryWan/blob/master/source/firm.c
*   ctrNandWrite logic adapted from https://github.com/d0k3/GodMode9/blob/master/source/nand/nand.c
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

#define REGs_AESTWLKEYS     (*((vu32 (*)[4][3][4])0x10009040))

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

#define AES_INPUT_BE            (AES_CNT_INPUT_ENDIAN)
#define AES_INPUT_LE            0
#define AES_INPUT_NORMAL        (AES_CNT_INPUT_ORDER)
#define AES_INPUT_REVERSED      0
#define AES_INPUT_TWLNORMAL     0
#define AES_INPUT_TWLREVERSED   (AES_CNT_INPUT_ORDER)

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

extern FirmwareSource firmSource;

void sha(void *res, const void *src, u32 size, u32 mode);

int ctrNandInit(void);
int ctrNandRead(u32 sector, u32 sectorCount, u8 *outbuf);
int ctrNandWrite(u32 sector, u32 sectorCount, const u8 *inbuf);
u32 decryptExeFs(Cxi *cxi);
u32 decryptNusFirm(const Ticket *ticket, Cxi *cxi, u32 ncchSize);
void setupKeyslots(void);
void kernel9Loader(Arm9Bin *arm9Section);
void computePinHash(u8 *outbuf, const u8 *inbuf);
