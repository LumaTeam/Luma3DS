#pragma once

#include "types.h"
#include "kernel.h"

typedef struct
{
    u32     bits1_0 : 2;    ///< 0b00
}   Desc_TranslationFault;

typedef struct
{
    u32     bits1_0 : 2;    ///< 0b01
    u32     sbz     : 3;
    u32     domain  : 4;
    u32     p       : 1;
    u32     addr    : 21;
}   Desc_CoarsePageTable;

typedef struct
{
    u32     bits1_0 : 2;    ///< 0b10
    u32     b       : 1;
    u32     c       : 1;
    u32     xn      : 1;
    u32     domain  : 4;
    u32     p       : 1;
    u32     ap      : 2;
    u32     tex     : 3;
    u32     apx     : 1;
    u32     s       : 1;
    u32     ng      : 1;
    u32     bit18   : 1;    ///< 0
    u32     sbz     : 1;
    u32     addr    : 12;
}   Desc_Section;

typedef struct
{
    u32     bits1_0 : 2;    ///< 0b10
    u32     b       : 1;
    u32     c       : 1;
    u32     xn      : 1;
    u32     domain  : 4;
    u32     p       : 1;
    u32     ap      : 2;
    u32     tex     : 3;
    u32     sbz     : 3;
    u32     bit18   : 1;    ///< 1
    u32     sbz2    : 5;
    u32     addr    : 8;
}   Desc_Supersection;

typedef struct
{
    u32     bits1_0 : 2;    ///< 0b11
}   Desc_Reserved;

typedef struct
{
    u32     bits1_0 : 2;    ///< 0b01
    u32     b       : 1;
    u32     c       : 1;
    u32     ap      : 2;
    u32     sbz     : 3;
    u32     apx     : 1;
    u32     s       : 1;
    u32     ng      : 1;
    u32     tex     : 3;
    u32     xn      : 1;
    u32     addr    : 16;
}   Desc_LargePage;

typedef struct
{
    u32     xn      : 1;
    u32     bit1    : 1;    ///< 1
    u32     b       : 1;
    u32     c       : 1;
    u32     ap      : 2;
    u32     tex     : 3;
    u32     apx     : 1;
    u32     s       : 1;
    u32     ng      : 1;
    u32     addr    : 20;
}   Desc_SmallPage;

typedef union
{
    u32     raw;

    Desc_TranslationFault   translationFault;
    Desc_CoarsePageTable    coarsePageTable;
    Desc_Section            section;
    Desc_Supersection       supersection;
    Desc_Reserved           reserved;

}   L1Descriptor;

typedef union
{
    u32     raw;

    Desc_TranslationFault   translationFault;
    Desc_LargePage          largePage;
    Desc_SmallPage          smallPage;
}   L2Descriptor;

typedef enum
{
    Descriptor_TranslationFault,
    Descriptor_CoarsePageTable,
    Descriptor_Section,
    Descriptor_Supersection,
    Descriptor_Reserved,
    Descriptor_LargePage,
    Descriptor_SmallPage
}   DescType;

void    L1MMUTable__RWXForAll(u32 *table);
void    L2MMUTable__RWXForAll(u32 *table);
u32     L1MMUTable__GetPAFromVA(u32 *table, u32 va);
u32     L2MMUTable__GetPAFromVA(u32 *table, u32 va);
u32     L1MMUTable__GetAddressUserPerm(u32 *table, u32 va);
u32     L2MMUTable__GetAddressUserPerm(u32 *table, u32 va);

void    KProcessHwInfo__SetMMUTableToRWX(KProcessHwInfo *hwInfo);
u32     KProcessHwInfo__GetPAFromVA(KProcessHwInfo *hwInfo, u32 va);
u32     KProcessHwInfo__GetAddressUserPerm(KProcessHwInfo *hwInfo, u32 va);
