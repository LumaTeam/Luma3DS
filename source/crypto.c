// From http://github.com/b1l1s/ctr

#include "crypto.h"

#include <stddef.h>
#include "memory.h"
#include "fatfs/sdmmc/sdmmc.h"
#include "fatfs/ff.h"

/****************************************************************
*                   Crypto Libs
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

void aes_setkey(u8 keyslot, const void* key, u32 keyType, u32 mode)
{
	if(keyslot <= 0x03) return; // Ignore TWL keys for now
	u32* key32 = (u32*)key;
	*REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)) | mode;
	*REG_AESKEYCNT = (*REG_AESKEYCNT >> 6 << 6) | keyslot | AES_KEYCNT_WRITE;

	REG_AESKEYFIFO[keyType] = key32[0];
	REG_AESKEYFIFO[keyType] = key32[1];
	REG_AESKEYFIFO[keyType] = key32[2];
	REG_AESKEYFIFO[keyType] = key32[3];
}

void aes_use_keyslot(u8 keyslot)
{
	if(keyslot > 0x3F)
		return;

	*REG_AESKEYSEL = keyslot;
	*REG_AESCNT = *REG_AESCNT | 0x04000000; /* mystery bit */
}

void aes_setiv(const void* iv, u32 mode)
{
	const u32* iv32 = (const u32*)iv;
	*REG_AESCNT = (*REG_AESCNT & ~(AES_CNT_INPUT_ENDIAN | AES_CNT_INPUT_ORDER)) | mode;

	// Word order for IV can't be changed in REG_AESCNT and always default to reversed
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

void aes_advctr(void* ctr, u32 val, u32 mode)
{
	u32* ctr32 = (u32*)ctr;
	
	int i;
	if(mode & AES_INPUT_BE)
	{
		for(i = 0; i < 4; ++i) // Endian swap
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
		for(i = 0; i < 4; ++i) // Endian swap
			BSWAP32(ctr32[i]);
	}
}

void aes_change_ctrmode(void* ctr, u32 fromMode, u32 toMode)
{
	u32* ctr32 = (u32*)ctr;
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

void aes_batch(void* dst, const void* src, u32 blockCount)
{
	*REG_AESBLKCNT = blockCount << 16;
	*REG_AESCNT |=	AES_CNT_START;
	
	const u32* src32	= (const u32*)src;
	u32* dst32			= (u32*)dst;
	
	u32 wbc = blockCount;
	u32 rbc = blockCount;
	
	while(rbc)
	{
		if(wbc && ((*REG_AESCNT & 0x1F) <= 0xC)) // There's space for at least 4 ints
		{
			*REG_AESWRFIFO = *src32++;
			*REG_AESWRFIFO = *src32++;
			*REG_AESWRFIFO = *src32++;
			*REG_AESWRFIFO = *src32++;
			wbc--;
		}
		
		if(rbc && ((*REG_AESCNT & (0x1F << 0x5)) >= (0x4 << 0x5))) // At least 4 ints available for read
		{
			*dst32++ = *REG_AESRDFIFO;
			*dst32++ = *REG_AESRDFIFO;
			*dst32++ = *REG_AESRDFIFO;
			*dst32++ = *REG_AESRDFIFO;
			rbc--;
		}
	}
}

void aes(void* dst, const void* src, u32 blockCount, void* iv, u32 mode, u32 ivMode)
{
	*REG_AESCNT =	mode |
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

		// Save the last block for the next decryption CBC batch's iv
		if((mode & AES_ALL_MODES) == AES_CBC_DECRYPT_MODE)
		{
			memcpy(iv, src + (blocks - 1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
			aes_change_ctrmode(iv, AES_INPUT_BE | AES_INPUT_NORMAL, ivMode);
		}

		// Process the current batch
		aes_batch(dst, src, blocks);

		// Save the last block for the next encryption CBC batch's iv
		if((mode & AES_ALL_MODES) == AES_CBC_ENCRYPT_MODE)
		{
			memcpy(iv, dst + (blocks - 1) * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
			aes_change_ctrmode(iv, AES_INPUT_BE | AES_INPUT_NORMAL, ivMode);
		}
		
		// Advance counter for CTR mode
		else if((mode & AES_ALL_MODES) == AES_CTR_MODE)
			aes_advctr(iv, blocks, ivMode);

		src += blocks * AES_BLOCK_SIZE;
		dst += blocks * AES_BLOCK_SIZE;
		blockCount -= blocks;
	}
}

void sha_wait_idle()
{
	while(*REG_SHA_CNT & 1);
}

void sha(void* res, const void* src, u32 size, u32 mode)
{
	sha_wait_idle();
	*REG_SHA_CNT = mode | SHA_CNT_OUTPUT_ENDIAN | SHA_NORMAL_ROUND;
	
	const u32* src32	= (const u32*)src;
	int i;
	while(size >= 0x40)
	{
		sha_wait_idle();
		for(i = 0; i < 4; ++i)
		{
			*REG_SHA_INFIFO = *src32++;
			*REG_SHA_INFIFO = *src32++;
			*REG_SHA_INFIFO = *src32++;
			*REG_SHA_INFIFO = *src32++;
		}

		size -= 0x40;
	}
	
	sha_wait_idle();
	memcpy((void*)REG_SHA_INFIFO, src32, size);
	
	*REG_SHA_CNT = (*REG_SHA_CNT & ~SHA_NORMAL_ROUND) | SHA_FINAL_ROUND;
	
	while(*REG_SHA_CNT & SHA_FINAL_ROUND);
	sha_wait_idle();
	
	u32 hashSize = SHA_256_HASH_SIZE;
	if(mode == SHA_224_MODE)
		hashSize = SHA_224_HASH_SIZE;
	else if(mode == SHA_1_MODE)
		hashSize = SHA_1_HASH_SIZE;

	memcpy(res, (void*)REG_SHA_HASH, hashSize);
}

void rsa_wait_idle()
{
	while(*REG_RSA_CNT & 1);
}

void rsa_use_keyslot(u32 keyslot)
{
	*REG_RSA_CNT = (*REG_RSA_CNT & ~RSA_CNT_KEYSLOTS) | (keyslot << 4);
}

void rsa_setkey(u32 keyslot, const void* mod, const void* exp, u32 mode)
{
	rsa_wait_idle();
	*REG_RSA_CNT = (*REG_RSA_CNT & ~RSA_CNT_KEYSLOTS) | (keyslot << 4) | RSA_IO_BE | RSA_IO_NORMAL;
	
	u32 size = mode * 4;
	
	volatile u32* keyslotCnt = REG_RSA_SLOT0 + (keyslot << 4);
	keyslotCnt[0] &= ~(RSA_SLOTCNT_KEY_SET | RSA_SLOTCNT_WPROTECT);
	keyslotCnt[1] = mode;
	
	memcpy((void*)REG_RSA_MOD_END - size, mod, size);
	
	if(exp == NULL)
	{
		size -= 4;
		while(size)
		{
			*REG_RSA_EXPFIFO = 0;
			size -= 4;
		}
		*REG_RSA_EXPFIFO = 0x01000100; // 0x00010001 byteswapped
	}
	else
	{
		const u32* exp32 = (const u32*)exp;
		while(size)
		{
			*REG_RSA_EXPFIFO = *exp32++;
			size -= 4;
		}
	}
}

int rsa_iskeyset(u32 keyslot)
{
	return *(REG_RSA_SLOT0 + (keyslot << 4)) & 1;
}

void rsa(void* dst, const void* src, u32 size)
{
	u32 keyslot = (*REG_RSA_CNT & RSA_CNT_KEYSLOTS) >> 4;
	if(rsa_iskeyset(keyslot) == 0)
		return;

	rsa_wait_idle();
	*REG_RSA_CNT |= RSA_IO_BE | RSA_IO_NORMAL;
	
	// Pad the message with zeroes so that it's a multiple of 8
	// and write the message with the end aligned with the register
	u32 padSize = ((size + 7) & ~7) - size;
	memset((void*)REG_RSA_TXT_END - (size + padSize), 0, padSize);
	memcpy((void*)REG_RSA_TXT_END - size, src, size);
	
	// Start
	*REG_RSA_CNT |= RSA_CNT_START;
	
	rsa_wait_idle();
	memcpy(dst, (void*)REG_RSA_TXT_END - size, size);
}

int rsa_verify(const void* data, u32 size, const void* sig, u32 mode)
{
	u8 dataHash[SHA_256_HASH_SIZE];
	sha(dataHash, data, size, SHA_256_MODE);
	
	u8 decSig[0x100]; // Way too big, need to request a work area
	
	u32 sigSize = mode * 4;
	rsa(decSig, sig, sigSize);
	
	return memcmp(dataHash, decSig + (sigSize - SHA_256_HASH_SIZE), SHA_256_HASH_SIZE) == 0;
}

/****************************************************************
*                   Nand/FIRM Crypto stuff
****************************************************************/

//Get Nand CTR key
void getNandCTR(u8 *buf) {
	u8 *addr = (u8*)0x080D8BBC;
    u8 keyLen = 0x10; //CTR length
	addr += 0x0F;
	while (keyLen --) { *(buf++) = *(addr--); }
}

//Read firm0 from NAND and write to buffer
void nandFirm0(u8 *outbuf, const u32 size){
    u8 CTR[0x10];
    getNandCTR(CTR);
    aes_advctr(CTR, 0x0B130000/0x10, AES_INPUT_BE | AES_INPUT_NORMAL);
    sdmmc_nand_readsectors(0x0B130000 / 0x200, size / 0x200, outbuf);
    aes_use_keyslot(0x06);
    aes(outbuf, outbuf, size / AES_BLOCK_SIZE, CTR, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);
}

//Emulates the Arm9loader process
//9.5.0 = 0x0F
//9.6.0 = 0x18
void arm9loader(void *armHdr, u32 kversion){
    //Set Nand key#2  here (decrypted from 0x12C10)
    u8 key2[0x10] = {0x42, 0x3F, 0x81, 0x7A, 0x23, 0x52, 0x58, 0x31, 0x6E, 0x75, 0x8E, 0x3A, 0x39, 0x43, 0x2E, 0xD0};
    
    u8 keyX[0x10];
    u8 keyY[0x10];
    u8 CTR[0x10];
    u32 slot = (kversion >= 0x0F ? 0x16 : 0x15);
    
    //Setupkeys needed for arm9bin decryption
    memcpy(keyY, armHdr+0x10, 0x10);
    memcpy(CTR, armHdr+0x20, 0x10);
    u32 size = atoi(armHdr+0x30);

    if(kversion >= 0x0F){
        if(kversion >= 0x18) aes_setkey(0x11, key2, AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_use_keyslot(0x11);
        aes(keyX, armHdr+0x60, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
        aes_setkey(slot, keyX, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    }
    
    aes_setkey(slot, keyY, AES_KEYY, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setiv(CTR, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_use_keyslot(slot);
    
    aes(armHdr+0x800, armHdr+0x800, size/AES_BLOCK_SIZE, CTR, AES_CTR_MODE, AES_INPUT_BE | AES_INPUT_NORMAL);
}

void setKeys(kversion){
    u8 key2[0x10] = {0x42, 0x3F, 0x81, 0x7A, 0x23, 0x52, 0x58, 0x31, 0x6E, 0x75, 0x8E, 0x3A, 0x39, 0x43, 0x2E, 0xD0};
   //Initialze keys
    if(kversion >= 0x18){

    u8 keyX18[16] = {0x82, 0xE9, 0xC9, 0xBE, 0xBF, 0xB8, 0xBD, 0xB8, 0x75, 0xEC, 0xC0, 0xA0, 0x7D, 0x47, 0x43, 0x74};
    u8 keyX19[16] = {0xF5, 0x36, 0x7F, 0xCE, 0x73, 0x14, 0x2E, 0x66, 0xED, 0x13, 0x91, 0x79, 0x14, 0xB7, 0xF2, 0xEF};
    u8 keyX1A[16] = {0xEA, 0xBA, 0x98, 0x4C, 0x9C, 0xB7, 0x66, 0xD4, 0xA3, 0xA7, 0xE9, 0x74, 0xE2, 0xE7, 0x13, 0xA3};
    u8 keyX1B[16] = {0x45, 0xAD, 0x04, 0x95, 0x39, 0x92, 0xC7, 0xC8, 0x93, 0x72, 0x4A, 0x9A, 0x7B, 0xCE, 0x61, 0x82};
    u8 keyX1C[16] = {0xC3, 0x83, 0x0F, 0x81, 0x56, 0xE3, 0x54, 0x3B, 0x72, 0x3F, 0x0B, 0xC0, 0x46, 0x74, 0x1E, 0x8F};
    u8 keyX1D[16] = {0xD6, 0xB3, 0x8B, 0xC7, 0x59, 0x41, 0x75, 0x96, 0xD6, 0x19, 0xD6, 0x02, 0x9D, 0x13, 0xE0, 0xD8};
    u8 keyX1E[16] = {0xBB, 0x62, 0x3A, 0x97, 0xDD, 0xD7, 0x93, 0xD7, 0x57, 0xC4, 0x10, 0x4B, 0x8D, 0x9F, 0xB9, 0x69};
    u8 keyX1F[16] = {0x4C, 0x28, 0xEC, 0x6E, 0xFF, 0xA3, 0xC2, 0x36, 0x46, 0x07, 0x8B, 0xBA, 0x35, 0x0C, 0x79, 0x95};
    aes_setkey(0x18, keyX18, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x19, keyX19, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x1A, keyX19, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x1B, keyX1B, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x1C, keyX1C, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x1D, keyX1D, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x1E, keyX1E, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
    aes_setkey(0x1F, keyX1F, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);

    /*
       //data at armHdr+0x8A804 (its not in FCRAM for whatever reason)
        u8 encryptedData1[0x10] = {
            0xA4, 0x8D, 0xE4, 0xF1, 0x0B, 0x36, 0x44, 0xAA, 0x90, 0x31, 0x28, 0xFF,
            0x4D, 0xCA, 0x76, 0xDF
        };
        //data at armHdr+0x8A814 (its not in FCRAM for whatever reason)
        u8 encryptedData2[0x10] = {
            0xDD, 0xDA, 0xA4, 0xC6, 0x2C, 0xC4, 0x50, 0xE9, 0xDA, 0xB6, 0x9B, 0x0D,
            0x9D, 0x2A, 0x21, 0x98
        };
        
        //Set key 0x18 keyX
        u8 keyX18[0x10];
        aes(keyX18, encryptedData1, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
        aes_setkey(0x18, keyX18, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        
        //Set key 0x11 normalkey
        aes_setkey(0x11, key2, AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_use_keyslot(0x11);
        
        //Set keys 0x19..0x1F keyXs
        u8 keyTemp[0x10];
        u8 keys[7][0x10];
        aes_use_keyslot(0x11);
        int i; for(i = 0; i < 7; i++) {
            aes(keyTemp, encryptedData2, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
            encryptedData2[0x0F]++;
            memcpy(keys[i], keyTemp, 0x10);
        }
        aes_setkey(0x19, keys[0], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_setkey(0x1A, keys[1], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_setkey(0x1B, keys[2], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_setkey(0x1C, keys[3], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_setkey(0x1D, keys[4], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_setkey(0x1E, keys[5], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_setkey(0x1F, keys[6], AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);*/
    }
        //Set key 0x18 keyX
        u8 keyX18[0x10];
        aes(keyX18, encryptedData1, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
        aes_setkey(0x18, keyX18, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
        
        //Set key 0x11 normalkey
        aes_setkey(0x11, key2, AES_KEYNORMAL, AES_INPUT_BE | AES_INPUT_NORMAL);
        aes_use_keyslot(0x11);
        
        //Set keys 0x19..0x1F keyXs
        u8 keyTemp[0x10];
        aes_use_keyslot(0x11);
        int i; for(i = 0; i < 7; i++) {
            aes(keyTemp, encryptedData2, 1, NULL, AES_ECB_DECRYPT_MODE, 0);
            aes_setkey(0x19 + i, keyTemp, AES_KEYX, AES_INPUT_BE | AES_INPUT_NORMAL);
            encryptedData2[0x0F]++;
        }*/
    }
}