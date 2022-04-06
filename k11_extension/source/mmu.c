#include "mmu.h"
#include "globals.h"
#include "utils.h"

extern u8 svcSignalingEnabled;

DescType     L1Descriptor__GetType(u32 descriptor)
{
    L1Descriptor   pdesc = {descriptor};

    if (pdesc.reserved.bits1_0 == 0b00)
        return Descriptor_TranslationFault;
    if (pdesc.reserved.bits1_0 == 0b01)
        return Descriptor_CoarsePageTable;
    if (pdesc.reserved.bits1_0 == 0b10)
        return pdesc.section.bit18 == 0 ? Descriptor_Section : Descriptor_Supersection;
    return Descriptor_Reserved;
}

DescType     L2Descriptor__GetType(u32 descriptor)
{
    L2Descriptor    pdesc = {descriptor};

    if (pdesc.translationFault.bits1_0 == 0b01)
        return Descriptor_LargePage;
    if (pdesc.smallPage.bit1 == 1)
        return Descriptor_SmallPage;

    return Descriptor_TranslationFault;
}

void    L1MMUTable__RWXForAll(u32 *table)
{
    u32     *tableEnd = table + 1024;

    for (; table != tableEnd; ++table)
    {
        L1Descriptor    descriptor = {*table};

        switch (L1Descriptor__GetType(descriptor.raw))
        {
            case Descriptor_CoarsePageTable:
            {
                u32     *l2table = (u32 *)((descriptor.coarsePageTable.addr << 10) - 0x40000000);

                L2MMUTable__RWXForAll(l2table);
                break;
            }
            case Descriptor_Section:
            {
                descriptor.section.xn = 0;
                descriptor.section.apx = 0;
                descriptor.section.ap = 3;
                *table = descriptor.raw;
                break;
            }
            case Descriptor_Supersection:
            {
                descriptor.supersection.xn = 0;
                descriptor.supersection.ap = 3;
                *table = descriptor.raw;
                break;
            }
            default:
                break;
        }
    }
}

void    L2MMUTable__RWXForAll(u32 *table)
{
    u32     *tableEnd = table + 256;

    for (; table != tableEnd; ++table)
    {
        L2Descriptor    descriptor = {*table};

        switch (L2Descriptor__GetType(descriptor.raw))
        {
            case Descriptor_LargePage:
            {
                descriptor.largePage.xn = 0;
                descriptor.largePage.apx = 0;
                descriptor.largePage.ap = 3;
                *table = descriptor.raw;
                break;
            }
            case Descriptor_SmallPage:
            {
                descriptor.smallPage.xn = 0;
                descriptor.smallPage.apx = 0;
                descriptor.smallPage.ap = 3;
                *table = descriptor.raw;
                break;
            }
            default:
                break;
        }
    }
}

u32     L1MMUTable__GetPAFromVA(u32 *table, u32 va)
{
    u32             pa = 0;
    L1Descriptor    descriptor = {table[va >> 20]};

    switch (L1Descriptor__GetType(descriptor.raw))
    {
        case Descriptor_CoarsePageTable:
        {
            u32     *l2table = (u32 *)((descriptor.coarsePageTable.addr << 10) - 0x40000000);

            pa = L2MMUTable__GetPAFromVA(l2table, va);
            break;
        }
        case Descriptor_Section:
        {
            pa = descriptor.section.addr << 20;
            pa |= (va << 12) >> 12;
            break;
        }
        case Descriptor_Supersection:
        {
            pa = descriptor.supersection.addr << 24;
            pa |= (va << 8) >> 8;
            break;
        }
        default:
            // VA not found
            break;
    }

    return pa;
}

u32     L2MMUTable__GetPAFromVA(u32 *table, u32 va)
{
    u32             pa = 0;
    L2Descriptor    descriptor = {table[(va << 12) >> 24]};

    switch(L2Descriptor__GetType(descriptor.raw))
    {
        case Descriptor_LargePage:
        {
            pa = descriptor.largePage.addr << 16;
            pa |= va & 0xFFFF;
            break;
        }
        case Descriptor_SmallPage:
        {
            pa = descriptor.smallPage.addr << 12;
            pa |= va & 0xFFF;
            break;
        }
        default:
            break;
    }

    return pa;
}

u32     L1MMUTable__GetAddressUserPerm(u32 *table, u32 va)
{
    u32             perm = 0;
    L1Descriptor    descriptor = {table[va >> 20]};

    switch (L1Descriptor__GetType(descriptor.raw))
    {
        case Descriptor_CoarsePageTable:
        {
            u32     *l2table = (u32 *)((descriptor.coarsePageTable.addr << 10) - 0x40000000);

            perm = L2MMUTable__GetAddressUserPerm(l2table, va);
            break;
        }
        case Descriptor_Section:
        {
            perm = descriptor.section.ap >> 1;

            if (perm)
            {
                perm |= (!descriptor.section.apx && (descriptor.section.ap & 1)) << 1;
                perm |= (!descriptor.section.xn) << 2;
            }
            break;
        }
        case Descriptor_Supersection:
        {
            perm = descriptor.supersection.ap >> 1;

            if (perm)
            {
                perm |= (descriptor.supersection.ap & 1) << 1;
                perm |= (!descriptor.supersection.xn) << 2;
            }
            break;
        }
        default:
            // VA not found
            break;
    }

    return perm;
}

u32     L2MMUTable__GetAddressUserPerm(u32 *table, u32 va)
{
    u32             perm = 0;
    L2Descriptor    descriptor = {table[(va << 12) >> 24]};

    switch(L2Descriptor__GetType(descriptor.raw))
    {
        case Descriptor_LargePage:
        {
            perm = descriptor.largePage.ap >> 1;
            if (perm)
            {
                perm |= (!descriptor.largePage.apx && (descriptor.largePage.ap & 1)) << 1;
                perm |= (!descriptor.largePage.xn) << 2;
            }
            break;
        }
        case Descriptor_SmallPage:
        {
            perm = descriptor.smallPage.ap >> 1;
            if (perm)
            {
                perm |= (!descriptor.smallPage.apx && (descriptor.smallPage.ap & 1)) << 1;
                perm |= (!descriptor.smallPage.xn) << 2;
            }
            break;
        }
        default:
            break;
    }

    return perm;
}

void    KProcessHwInfo__SetMMUTableToRWX(KProcessHwInfo *hwInfo)
{
    KObjectMutex    *mutex = KPROCESSHWINFO_GET_PTR(hwInfo, mutex);
    u32             *table = KPROCESSHWINFO_GET_RVALUE(hwInfo, mmuTableVA);

    KObjectMutex__Acquire(mutex);

    L1MMUTable__RWXForAll(table);

    KObjectMutex__Release(mutex);
}

u32     KProcessHwInfo__GetPAFromVA(KProcessHwInfo *hwInfo, u32 va)
{
    KObjectMutex    *mutex = KPROCESSHWINFO_GET_PTR(hwInfo, mutex);
    u32             *table = KPROCESSHWINFO_GET_RVALUE(hwInfo, mmuTableVA);

    KObjectMutex__Acquire(mutex);

    u32 pa = L1MMUTable__GetPAFromVA(table, va);

    KObjectMutex__Release(mutex);

    return pa;
}

u32     KProcessHwInfo__GetAddressUserPerm(KProcessHwInfo *hwInfo, u32 va)
{
    KObjectMutex    *mutex = KPROCESSHWINFO_GET_PTR(hwInfo, mutex);
    u32             *table = KPROCESSHWINFO_GET_RVALUE(hwInfo, mmuTableVA);

    KObjectMutex__Acquire(mutex);

    u32 perm = L1MMUTable__GetAddressUserPerm(table, va);

    KObjectMutex__Release(mutex);

    return perm;
}

static union
{
    u32     raw;
    struct
    {
        u32     xn      : 1;
        u32     unkn    : 1;
        u32     cb      : 2;
        u32     ap      : 2;
        u32     tex     : 3;
        u32     apx     : 1;
        u32     s       : 1;
        u32     ng      : 1;
    };
} g_rwxState;

// This function patch the permissions when memory is mapped in the mmu table (rwx)
KProcessHwInfo *PatchDescriptorAccessControl(KProcessHwInfo *hwInfo, u32 **outState)
{
    KProcess    *process = (KProcess *)((u32)hwInfo - 0x1C);
    u32 state = **outState;
    u32 flags =  KPROCESS_GET_RVALUE(process, customFlags);

    if (flags & SignalOnMemLayoutChanges) {
        svcSignalingEnabled |= 2;
        *KPROCESS_GET_PTR(process, customFlags) |= MemLayoutChanged;
    }        

    if (!(flags & ForceRWXPages))
        return hwInfo;

    g_rwxState.raw = state;
    g_rwxState.xn = 0;
    g_rwxState.ap = 3;
    g_rwxState.apx = 0;

    *outState = &g_rwxState.raw;

    return hwInfo;
}
