#pragma once

#include <3ds/types.h>

typedef struct
{
    u8 reserved[5];
    u8 flag;
    u8 remasterversion[2];
} PACKED exheader_systeminfoflags;

typedef struct
{
    u32 address;
    u32 nummaxpages;
    u32 codesize;
} PACKED exheader_codesegmentinfo;

typedef struct
{
    u8 name[8];
    exheader_systeminfoflags flags;
    exheader_codesegmentinfo text;
    u8 stacksize[4];
    exheader_codesegmentinfo ro;
    u8 reserved[4];
    exheader_codesegmentinfo data;
    u32 bsssize;
} PACKED exheader_codesetinfo;

typedef struct
{
    u64 programid[0x30];
} PACKED exheader_dependencylist;

typedef struct
{
    u8 savedatasize[4];
    u8 reserved[4];
    u8 jumpid[8];
    u8 reserved2[0x30];
} PACKED exheader_systeminfo;

typedef struct
{
    u8 extsavedataid[8];
    u8 systemsavedataid[8];
    u8 reserved[8];
    u8 accessinfo[7];
    u8 otherattributes;
} PACKED exheader_storageinfo;

typedef struct
{
    u64 programid;
    u32 firm;
    u8 flags[3];
    u8 priority;
    u16 resourcelimitdescriptor[0x10];
    exheader_storageinfo storageinfo;
    u64 serviceaccesscontrol[0x22];
    u8 reserved[0xf];
    u8 resourcelimitcategory;
} PACKED exheader_arm11systemlocalcaps;

typedef struct
{
    u32 descriptors[28];
    u8 reserved[0x10];
} PACKED exheader_arm11kernelcapabilities;

typedef struct
{
    u8 descriptors[15];
    u8 descversion;
} PACKED exheader_arm9accesscontrol;

typedef struct
{
    // systemcontrol info {
    //   coreinfo {
    exheader_codesetinfo codesetinfo;
    exheader_dependencylist deplist;
    //   }
    exheader_systeminfo systeminfo;
    // }
    // accesscontrolinfo {
    exheader_arm11systemlocalcaps arm11systemlocalcaps;
    exheader_arm11kernelcapabilities arm11kernelcaps;
    exheader_arm9accesscontrol arm9accesscontrol;
    // }
    struct {
        u8 signature[0x100];
        u8 ncchpubkeymodulus[0x100];
        exheader_arm11systemlocalcaps arm11systemlocalcaps;
        exheader_arm11kernelcapabilities arm11kernelcaps;
        exheader_arm9accesscontrol arm9accesscontrol;
    } PACKED accessdesc;
} PACKED exheader_header;
