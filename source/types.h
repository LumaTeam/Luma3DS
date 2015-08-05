/*
*   types.h
*       by Reisyukaku
*/
#ifndef TYPES_INC
#define TYPES_INC

#include <stdint.h>
#include <stdlib.h>

//Common data types
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

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

#endif