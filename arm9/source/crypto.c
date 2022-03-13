/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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

#include "crypto.h"
#include "memory.h"
#include "emunand.h"
#include "utils.h"
#include "alignedseqmemcpy.h"
#include "strings.h"
#include "fatfs/sdmmc/sdmmc.h"

/****************************************************************
*                  Crypto libs
****************************************************************/

/* original version by megazig */

#ifndef __thumb__
#define BSWAP32(x) {\
    __asm__\
    (\
        "eor r1, %1, %1, ror #16\n\t"\
        "bic r1, r1, #0xFF0000\n\t"\
        "mov %0, %1, ror #8\n\t"\
        "eor %0, %0, r1, lsr #8\n\t"\
        :"=r"(x)\
        :"0"(x)\
        :"r1"\
    );\
};

#define ADD_u128_u32(u128_0, u128_1, u128_2, u128_3, u32_0) {\
__asm__\
    (\
        "adds %0, %4\n\t"\
        "addcss %1, %1, #1\n\t"\
        "addcss %2, %2, #1\n\t"\
        "addcs %3, %3, #1\n\t"\
        : "+r"(u128_0), "+r"(u128_1), "+r"(u128_2), "+r"(u128_3)\
        : "r"(u32_0)\
        : "cc"\
    );\
}
#else
#define BSWAP32(x) {x = __builtin_bswap32(x);}

#define ADD_u128_u32(u128_0, u128_1, u128_2, u128_3, u32_0) {\
__asm__\
    (\
        "mov r4, #0\n\t"\
        "add %0, %0, %4\n\t"\
        "adc %1, %1, r4\n\t"\
        "adc %2, %2, r4\n\t"\
        "adc %3, %3, r4\n\t"\
        : "+r"(u128_0), "+r"(u128_1), "+r"(u128_2), "+r"(u128_3)\
        : "r"(u32_0)\
        : "cc", "r4"\
    );\
}
#endif /*__thumb__*/

static void aes_setkey(u8 keyslot, const void *key, u32 keyType, u32 mode)
{
    u32 *key32 = (u32 *)key;
    *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)) | mode;

    if(keyslot <= 3)
    {
        if((mode & AES_CNT_INPUT_ORDER) == AES_INPUT_TWLREVERSED)
        {
            REGs_AESTWLKEYS[keyslot][keyType][0] = key32[3];
            REGs_AESTWLKEYS[keyslot][keyType][1] = key32[2];
            REGs_AESTWLKEYS[keyslot][keyType][2] = key32[1];
            REGs_AESTWLKEYS[keyslot][keyType][3] = key32[0];
        }
        else
        {
            REGs_AESTWLKEYS[keyslot][keyType][0] = key32[0];
            REGs_AESTWLKEYS[keyslot][keyType][1] = key32[1];
            REGs_AESTWLKEYS[keyslot][keyType][2] = key32[2];
            REGs_AESTWLKEYS[keyslot][keyType][3] = key32[3];
        }
    }

    else if(keyslot < 0x40)
    {
        *REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | AES_KEYCNT_WRITE;

        REG_AESKEYFIFO[keyType] = key32[0];
        REG_AESKEYFIFO[keyType] = key32[1];
        REG_AESKEYFIFO[keyType] = key32[2];
        REG_AESKEYFIFO[keyType] = key32[3];
    }
}

static void aes_use_keyslot(u8 keyslot)
{
    if(keyslot > 0x3F)
        return;

    *REG_AESKEYSEL = keyslot;
    *REG_AESCNT = *REG_AESCNT | 0x04000000; /* mystery bit */
}

static void aes_setiv(const void *iv, u32 mode)
{
    const u32 *iv32 = (const u32 *)iv;
    *REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)) | mode;

    //Word order for IV can't be changed in REG_AESCNT and always default to reversed
    if(mode & AES_INPUT_NORMAL)
    {
        REG_AESCTR[0] = iv32[3];
        REG_AESCTR[1] = iv32[2];
        REG_AESCTR[2] = iv32[1];
        REG_AESCTR[3] = iv32[0];
    }
    else
    {
        REG_AESCTR[0] = iv32[0];
        REG_AESCTR[1] = iv32[1];
        REG_AESCTR[2] = iv32[2];
        REG_AESCTR[3] = iv32[3];
    }
}

static void aes_advctr(void *ctr, u32 val, u32 mode)
{
    u32 *ctr32 = (u32 *)ctr;

    int i;
    if(mode & AES_INPUT_BE)
    {
        for(i = 0; i < 4; ++i) //Endian swap
            BSWAP32(ctr32[i]);
    }

    if(mode & AES_INPUT_NORMAL)
    {
        ADD_u128_u32(ctr32[3], ctr32[2], ctr32[1], ctr32[0], val);
    }
    else
    {
        ADD_u128_u32(ctr32[0], ctr32[1], ctr32[2], ctr32[3], val);
    }

    if(mode & AES_INPUT_BE)
    {
        for(i = 0; i < 4; ++i) //Endian swap
            BSWAP32(ctr32[i]);
    }
}

static void aes_change_ctrmode(void *ctr, u32 fromMode, u32 toMode)
{
    u32 *ctr32 = (u32 *)ctr;
    int i;
    if((fromMode ^ toMode) & AES_CNT_INPUT_ENDIAN)
    {
        for(i = 0; i < 4; ++i)
            BSWAP32(ctr32[i]);
    }

    if((fromMode ^ toMode) & AES_CNT_INPUT_ORDER)
    {
        u32 temp = ctr32[0];
        ctr32[0] = ctr32[3];
        ctr32[3] = temp;

        temp = ctr32[1];
        ctr32[1] = ctr32[2];
        ctr32[2] = temp;
    }
}

static void aes_batch(void *dst, const void *src, u32 blockCount)
{
    *REG_AESBLKCNT = blockCount << 16;
    *REG_AESCNT |=  AES_CNT_START;

    const u32 *src32    = (const u32 *)src;
    u32 *dst32          = (u32 *)dst;

    u32 wbc = blockCount;
    u32 rbc = blockCount;

    while(rbc)
    {
        if(wbc && ((*REG_AESCNT & 0x1F) <= 0xC)) //There's space for at least 4 ints
        {
            *REG_AESWRFIFO = *src32++;
            *REG_AESWRFIFO = *src32++;
            *REG_AESWRFIFO = *src32++;
            *REG_AESWRFIFO = *src32++;
            wbc--;
        }

        if(rbc && ((*REG_AESCNT & (0x1F << 0x5)) >= (0x4 << 0x5))) //At least 4 ints available for read
        {
            *dst32++ = *REG_AESRDFIFO;
            *dst32++ = *REG_AESRDFIFO;
            *dst32++ = *REG_AESRDFIFO;
            *dst32++ = *REG_AESRDFIFO;
            rbc--;
        }
    }
}

static void aes(void *dst, const void *src, u32 blockCount, void *iv, u32 mode, u32 ivMode)
{
    *REG_AESCNT =   mode |
                    AES_CNT_INPUT_ORDER | AES_CNT_OUTPUT_ORDER |
                    AES_CNT_INPUT_ENDIAN | AES_CNT_OUTPUT_ENDIAN |
                    AES_CNT_FLUSH_READ | AES_CNT_FLUSH_WRITE;

    u32 blocks;
    while(blockCount != 0)
    {
        if((mode & AES_ALL_MODES) != AES_ECB_ENCRYPT_MODE
        && (mode & AES_ALL_MODES) != AES_ECB_DECRYPT_MODE)
            aes_setiv(iv, ivMode);

        blocks = (blockCount >= 0xFFFF) ? 0xFFFF : blockCount;

        //Save the last block for the next decryption CBC batch's iv
        if((mode & AES_ALL_MODES) == AES_CBC_DECRYPT_MODE)
        {
            memcpy(iv, src + (blocks - 1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
            aes_change_ctrmode(iv, AES_INPUT_BE | AES_INPUT_NORMAL, ivMode);
        }

        //Process the current batch
        aes_batch(dst, src, blocks);

        //Save the last block for the next encryption CBC batch's iv
        if((mode & AES_ALL_MODES) == AES_CBC_ENCRYPT_MODE)
        {
            memcpy(iv, dst + (blocks - 1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
            aes_change_ctrmode(iv, AES_INPUT_BE | AES_INPUT_NORMAL, ivMode);
        }

        //Advance counter for CTR mode
        else if((mode & AES_ALL_MODES) == AES_CTR_MODE)
            aes_advctr(iv, blocks, ivMode);

        src += blocks * AES_BLOCK_SIZE;
        dst += blocks * AES_BLOCK_SIZE;
        blockCount -= blocks;
    }
}

static void sha_wait_idle()
{
    while(*REG_SHA_CNT & 1);
}

void sha(void *res, const void *src, u32 size, u32 mode)
{
    sha_wait_idle();
    *REG_SHA_CNT = mode | SHA_CNT_OUTPUT_ENDIAN | SHA_NORMAL_ROUND;

    const u8 *src8 = (const u8 *)src;
    while(size >= 0x40)
    {
        sha_wait_idle();
        alignedseqmemcpy((void *)REG_SHA_INFIFO, src8, 0x40);

        src8 += 0x40;
        size -= 0x40;
    }

    sha_wait_idle();
    alignedseqmemcpy((void *)REG_SHA_INFIFO, src8, size);

    *REG_SHA_CNT = (*REG_SHA_CNT & ~SHA_NORMAL_ROUND) | SHA_FINAL_ROUND;

    while(*REG_SHA_CNT & SHA_FINAL_ROUND);
    sha_wait_idle();

    u32 hashSize = SHA_256_HASH_SIZE;
    if(mode == SHA_224_MODE)
        hashSize = SHA_224_HASH_SIZE;
    else if(mode == SHA_1_MODE)
        hashSize = SHA_1_HASH_SIZE;

    alignedseqmemcpy(res, (void *)REG_SHA_HASH, hashSize);
}

/*****************************************************************/

__attribute__((aligned(4))) static u8 nandCtr[AES_BLOCK_SIZE];
static u8 nandSlot;
static u32 fatStart = 0;

FirmwareSource firmSource = FIRMWARE_SYSNAND;

__attribute__((aligned(4))) static const u8 key1s[2][AES_BLOCK_SIZE] = {
    {0x07, 0x29, 0x44, 0x38, 0xF8, 0xC9, 0x75, 0x93, 0xAA, 0x0E, 0x4A, 0xB4, 0xAE, 0x84, 0xC1, 0xD8},
    {0xA2, 0xF4, 0x00, 0x3C, 0x7A, 0x95, 0x10, 0x25, 0xDF, 0x4E, 0x9E, 0x74, 0xE3, 0x0C, 0x92, 0x99}
},
                                            key2s[2][AES_BLOCK_SIZE] = {
    {0x42, 0x3F, 0x81, 0x7A, 0x23, 0x52, 0x58, 0x31, 0x6E, 0x75, 0x8E, 0x3A, 0x39, 0x43, 0x2E, 0xD0},
    {0xFF, 0x77, 0xA0, 0x9A, 0x99, 0x81, 0xE9, 0x48, 0xEC, 0x51, 0xC9, 0x32, 0x5D, 0x14, 0xEC, 0x25}
};

int ctrNandInit(void)
{
    __attribute__((aligned(4))) u8 cid[AES_BLOCK_SIZE],
                                   shaSum[SHA_256_HASH_SIZE];

    sdmmc_get_cid(1, (u32 *)cid);
    sha(shaSum, cid, sizeof(cid), SHA_256_MODE);
    memcpy(nandCtr, shaSum, sizeof(nandCtr));

    nandSlot = ISN3DS ? 0x05 : 0x04;

    int result;
    u8 __attribute__((aligned(4))) temp[0x200];

    //Read NCSD header
    result = firmSource == FIRMWARE_SYSNAND ? sdmmc_nand_readsectors(0, 1, temp) : sdmmc_sdcard_readsectors(emuOffset + emuHeader, 1, temp);

    if(!result)
    {
        u32 partitionNum = 1; //TWL partitions need to be first
        for(u8 *partitionId = temp + 0x111; *partitionId != 1; partitionId++, partitionNum++);

        u32 ctrMbrOffset = *((u32 *)(temp + 0x120) + (2 * partitionNum));

        //Read CTR MBR
        result = ctrNandRead(ctrMbrOffset, 1, temp);

        //Calculate final CTRNAND FAT offset
        if(!result) fatStart = ctrMbrOffset + *(u32 *)(temp + 0x1C6);
    }

    return result;
}

int ctrNandRead(u32 sector, u32 sectorCount, u8 *outbuf)
{
    __attribute__((aligned(4))) u8 tmpCtr[sizeof(nandCtr)];
    memcpy(tmpCtr, nandCtr, sizeof(nandCtr));
    aes_advctr(tmpCtr, ((sector + fatStart) * 0x200) / AES_BLOCK_SIZE, AES_INPUT_BE | AES_INPUT_NORMAL);

    //Read
    int result;
    if(firmSource == FIRMWARE_SYSNAND)
        result = sdmmc_nand_readsectors(sector + fatStart, sectorCount, outbuf);
    else
    {
        sector += emuOffset;
        result = sdmmc_sdcard_readsectors(sector + fatStart, sectorCount, outbuf);
    }

    //Decrypt
    aes_use_keyslot(nandSlot);
    aes(outbuf, outbuf, sectorCount * 0x200 / AES_BLOCK_SIZE, tmpCtr, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

    return result;
}

int ctrNandWrite(u32 sector, u32 sectorCount, const u8 *inbuf)
{
    u8 *buffer = (u8 *)0xFFF00000;
    u32 bufferSize = 0x4000;

    __attribute__((aligned(4))) u8 tmpCtr[sizeof(nandCtr)];
    memcpy(tmpCtr, nandCtr, sizeof(nandCtr));
    aes_advctr(tmpCtr, ((sector + fatStart) * 0x200) / AES_BLOCK_SIZE, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(nandSlot);

    int result = 0;
    for(u32 tempSector = 0; tempSector < sectorCount && !result; tempSector += bufferSize / 0x200)
    {
        u32 tempCount = (bufferSize / 0x200) < (sectorCount - tempSector) ? (bufferSize / 0x200) : (sectorCount - tempSector);

        memcpy(buffer, inbuf + (tempSector * 0x200), tempCount * 0x200);

        //Encrypt
        aes(buffer, buffer, tempCount * 0x200 / AES_BLOCK_SIZE, tmpCtr, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

        //Write
        result = sdmmc_nand_writesectors(tempSector + sector + fatStart, tempCount, buffer);
    }

    return result;
}

u32 decryptExeFs(Cxi *cxi)
{
    if(memcmp(cxi->ncch.magic, "NCCH", 4) != 0) return 0;

    if(cxi->ncch.exeFsOffset != 5) return 0;

    u8 *exeFsOffset = (u8 *)cxi + 6 * 0x200;
    u32 exeFsSize = (cxi->ncch.exeFsSize - 1) * 0x200;

    if(exeFsSize > 0x400000) return 0;

    __attribute__((aligned(4))) u8 ncchCtr[AES_BLOCK_SIZE] = {0};

    for(u32 i = 0; i < 8; i++)
        ncchCtr[7 - i] = cxi->ncch.partitionId[i];
    ncchCtr[8] = 2;

    aes_setkey(0x2C, cxi, AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_advctr(ncchCtr, 0x200 / AES_BLOCK_SIZE, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(0x2C);
    aes(cxi, exeFsOffset, exeFsSize / AES_BLOCK_SIZE, ncchCtr, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

    return memcmp(cxi, "FIRM", 4) == 0 ? exeFsSize : 0;
}

u32 decryptNusFirm(const Ticket *ticket, Cxi *cxi, u32 ncchSize)
{
    if(memcmp(ticket->sigIssuer, "Root", 4) != 0) return 0;

    __attribute__((aligned(4))) static const u8 keyY0x3D[AES_BLOCK_SIZE] = {0x0C, 0x76, 0x72, 0x30, 0xF0, 0x99, 0x8F, 0x1C, 0x46, 0x82, 0x82, 0x02, 0xFA, 0xAC, 0xBE, 0x4C};
    __attribute__((aligned(4))) u8 titleKey[AES_BLOCK_SIZE],
                                   cetkIv[AES_BLOCK_SIZE] = {0};
    memcpy(titleKey, ticket->titleKey, sizeof(titleKey));
    memcpy(cetkIv, ticket->titleId, sizeof(ticket->titleId));

    aes_setkey(0x3D, keyY0x3D, AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(0x3D);
    aes(titleKey, titleKey, 1, cetkIv, AES_CBC_DECRYPT_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

    __attribute__((aligned(4))) u8 ncchIv[AES_BLOCK_SIZE] = {0};

    aes_setkey(0x16, titleKey, AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(0x16);
    aes(cxi, cxi, ncchSize / AES_BLOCK_SIZE, ncchIv, AES_CBC_DECRYPT_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

    return decryptExeFs(cxi);
}

static inline void twlConsoleInfoInit(void)
{
    u64 twlConsoleId = ISDEVUNIT ? OTP_DEVCONSOLEID : (0x80000000ULL | (*(vu64 *)0x01FFB808 ^ 0x8C267B7B358A6AFULL));
    CFG_TWLUNITINFO = CFG_UNITINFO;
    OTP_TWLCONSOLEID = twlConsoleId;

    *REG_AESCNT = 0;

    vu32 *k3X = REGs_AESTWLKEYS[3][1],
         *k1X = REGs_AESTWLKEYS[1][1];

    k3X[0] = (u32)twlConsoleId;
    k3X[3] = (u32)(twlConsoleId >> 32);

    k1X[2] = (u32)(twlConsoleId >> 32);
    k1X[3] = (u32)twlConsoleId;

    aes_setkey(2, (u8 *)0x01FFD398, AES_KEYX, AES_INPUT_TWLNORMAL);
    if(CFG_TWLUNITINFO != 0)
    {
        __attribute__((aligned(4))) static const u8 key2YDev[AES_BLOCK_SIZE] = {0x3B, 0x06, 0x86, 0x57, 0x33, 0x04, 0x88, 0x11, 0x49, 0x04, 0x6B, 0x33, 0x12, 0x02, 0xAC, 0xF3},
                                                    key3YDev[AES_BLOCK_SIZE] = {0xAA, 0xBF, 0x76, 0xF1, 0x7A, 0xB8, 0xE8, 0x66, 0x97, 0x64, 0x6A, 0x26, 0x05, 0x00, 0xA0, 0xE1};

        k3X[1] = 0xEE7A4B1E;
        k3X[2] = 0xAF42C08B;
        aes_setkey(2, key2YDev, AES_KEYY, AES_INPUT_TWLNORMAL);
        aes_setkey(3, key3YDev, AES_KEYY, AES_INPUT_TWLNORMAL);
    }
    else
    {
        u32 last3YWord = 0xE1A00005;
        __attribute__((aligned(4))) u8 key3YRetail[AES_BLOCK_SIZE];

        memcpy(key3YRetail, (u8 *)0x01FFD3C8, 12);
        memcpy(key3YRetail + 12, &last3YWord, 4);

        k3X[1] = *(vu32 *)0x01FFD3A8; //"NINT"
        k3X[2] = *(vu32 *)0x01FFD3AC; //"ENDO"
        aes_setkey(2, (u8 *)0x01FFD220, AES_KEYY, AES_INPUT_TWLNORMAL);
        aes_setkey(3, key3YRetail, AES_KEYY, AES_INPUT_TWLNORMAL);
    }
}

void setupKeyslots(void)
{
    //Setup 0x24 KeyY
    __attribute__((aligned(4))) static const u8 keyY0x24[AES_BLOCK_SIZE] = {0x74, 0xCA, 0x07, 0x48, 0x84, 0xF4, 0x22, 0x8D, 0xEB, 0x2A, 0x1C, 0xA7, 0x2D, 0x28, 0x77, 0x62};
    aes_setkey(0x24, keyY0x24, AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);

    //Setup 0x25 KeyX and 0x2F KeyY
    __attribute__((aligned(4))) static const u8 keyX0x25s[2][AES_BLOCK_SIZE] = {
        {0xCE, 0xE7, 0xD8, 0xAB, 0x30, 0xC0, 0x0D, 0xAE, 0x85, 0x0E, 0xF5, 0xE3, 0x82, 0xAC, 0x5A, 0xF3},
        {0x81, 0x90, 0x7A, 0x4B, 0x6F, 0x1B, 0x47, 0x32, 0x3A, 0x67, 0x79, 0x74, 0xCE, 0x4A, 0xD7, 0x1B}
    },
                                                keyY0x2Fs[2][AES_BLOCK_SIZE] = {
        {0xC3, 0x69, 0xBA, 0xA2, 0x1E, 0x18, 0x8A, 0x88, 0xA9, 0xAA, 0x94, 0xE5, 0x50, 0x6A, 0x9F, 0x16},
        {0x73, 0x25, 0xC4, 0xEB, 0x14, 0x3A, 0x0D, 0x5F, 0x5D, 0xB6, 0xE5, 0xC5, 0x7A, 0x21, 0x95, 0xAC}
    };

    aes_setkey(0x25, keyX0x25s[ISDEVUNIT ? 1 : 0], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x2F, keyY0x2Fs[ISDEVUNIT ? 1 : 0], AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);

    if(ISN3DS)
    {
        //Setup 0x05 KeyY
        __attribute__((aligned(4))) static const u8 keyY0x5[AES_BLOCK_SIZE] =  {0x4D, 0x80, 0x4F, 0x4E, 0x99, 0x90, 0x19, 0x46, 0x13, 0xA2, 0x04, 0xAC, 0x58, 0x44, 0x60, 0xBE};
        aes_setkey(0x05, keyY0x5,  AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);
    }

    //Setup TWL keys
    twlConsoleInfoInit();

    __attribute__((aligned(4))) u8 keyBlocks[2][AES_BLOCK_SIZE] = {
        {0xA4, 0x8D, 0xE4, 0xF1, 0x0B, 0x36, 0x44, 0xAA, 0x90, 0x31, 0x28, 0xFF, 0x4D, 0xCA, 0x76, 0xDF},
        {0xDD, 0xDA, 0xA4, 0xC6, 0x2C, 0xC4, 0x50, 0xE9, 0xDA, 0xB6, 0x9B, 0x0D, 0x9D, 0x2A, 0x21, 0x98}
    },                             decKey[AES_BLOCK_SIZE];

    //Initialize Key 0x18
    aes_setkey(0x11, key1s[ISDEVUNIT ? 1 : 0], AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(0x11);
    aes(decKey, keyBlocks[0], 1, NULL, AES_ECB_DECRYPT_MODE, 0);
    aes_setkey(0x18, decKey, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);

    //Initialize Key 0x19-0x1F
    aes_setkey(0x11, key2s[ISDEVUNIT ? 1 : 0], AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(0x11);
    for(u8 slot = 0x19; slot < 0x20; slot++, keyBlocks[1][0xF]++)
    {
        aes(decKey, keyBlocks[1], 1, NULL, AES_ECB_DECRYPT_MODE, 0);
        aes_setkey(slot, decKey, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    }
}

void kernel9Loader(Arm9Bin *arm9Section)
{
    //Determine the kernel9loader version
    u32 k9lVersion;
    switch(arm9Section->magic[3])
    {
        case 0xFF:
            k9lVersion = 0;
            break;
        case '1':
            k9lVersion = 1;
            break;
        default:
            k9lVersion = 2;
    }

    u32 *startOfArm9Bin = (u32 *)((u8 *)arm9Section + 0x800);
    if(*startOfArm9Bin == 0x47704770 || *startOfArm9Bin == 0xB0862000) return; //Already decrypted

    aes_setkey(0x11, k9lVersion == 2 ? key2s[ISDEVUNIT ? 1 : 0] : key1s[ISDEVUNIT ? 1 : 0], AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);

    u8 arm9BinSlot = k9lVersion == 0 ? 0x15 : 0x16;

    // Get size
    u32 arm9SectionSize = decAtoi(arm9Section->size, 8);

    //Set keyX
    __attribute__((aligned(4))) u8 keyX[AES_BLOCK_SIZE];
    aes_use_keyslot(0x11);
    aes(keyX, k9lVersion == 0 ? arm9Section->keyX : arm9Section->slot0x16keyX, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
    aes_setkey(arm9BinSlot, keyX, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);

    //Set keyY
    __attribute__((aligned(4))) u8 keyY[AES_BLOCK_SIZE];
    memcpy(keyY, arm9Section->keyY, sizeof(keyY));
    aes_setkey(arm9BinSlot, keyY, AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);

    //Set CTR
    __attribute__((aligned(4))) u8 arm9BinCtr[AES_BLOCK_SIZE];
    memcpy(arm9BinCtr, arm9Section->ctr, sizeof(arm9BinCtr));

    //Decrypt Arm9 binary
    aes_use_keyslot(arm9BinSlot);
    aes(startOfArm9Bin, startOfArm9Bin, arm9SectionSize / AES_BLOCK_SIZE, arm9BinCtr, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);

    if(*startOfArm9Bin != 0x47704770 && *startOfArm9Bin != 0xB0862000) error("Failed to decrypt the Arm9 binary.");
}

void computePinHash(u8 *outbuf, const u8 *inbuf)
{
    __attribute__((aligned(4))) u8 cid[AES_BLOCK_SIZE],
                                   cipherText[AES_BLOCK_SIZE];

    sdmmc_get_cid(1, (u32 *)cid);
    aes_use_keyslot(0x04); //Console-unique keyslot whose keys are set by the Arm9 bootROM
    aes(cipherText, inbuf, 1, cid, AES_CBC_ENCRYPT_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);
    sha(outbuf, cipherText, sizeof(cipherText), SHA_256_MODE);
}
