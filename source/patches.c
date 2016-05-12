/*
*   patches.c
*/

#include "patches.h"
#include "memory.h"

/**************************************************
*                   Patches
**************************************************/

const u32 mpuPatch[3] = {0x00360003, 0x00200603, 0x001C0603};

const u16 nandRedir[2] = {0x4C00, 0x47A0},
          sigPatch[2] = {0x2000, 0x4770},
          writeBlock[2] = {0x2000, 0x46C0},
          writeBlockSafe[2] = {0x2400, 0xE01D};

//Official implementation of svcBackdoor
const u8  svcBackdoor[40] = {0xFF, 0x10, 0xCD, 0xE3,  //bic   r1, sp, #0xff
                             0x0F, 0x1C, 0x81, 0xE3,  //orr   r1, r1, #0xf00
                             0x28, 0x10, 0x81, 0xE2,  //add   r1, r1, #0x28
                             0x00, 0x20, 0x91, 0xE5,  //ldr   r2, [r1]
                             0x00, 0x60, 0x22, 0xE9,  //stmdb r2!, {sp, lr}
                             0x02, 0xD0, 0xA0, 0xE1,  //mov   sp, r2
                             0x30, 0xFF, 0x2F, 0xE1,  //blx   r0
                             0x03, 0x00, 0xBD, 0xE8,  //pop   {r0, r1}
                             0x00, 0xD0, 0xA0, 0xE1,  //mov   sp, r0
                             0x11, 0xFF, 0x2F, 0xE1}; //bx    r1

/**************************************************
*                   Functions
**************************************************/

u8 *getProcess9(u8 *pos, u32 size, u32 *process9Size, u32 *process9MemAddr)
{
    u8 *off = memsearch(pos, "ess9", size, 4);

    *process9Size = *(u32 *)(off - 0x60) * 0x200;
    *process9MemAddr = *(u32 *)(off + 0xC);

    //Process9 code offset (start of NCCH + ExeFS offset + ExeFS header size)
    return off - 0x204 + (*(u32 *)(off - 0x64) * 0x200) + 0x200;
}

void getSigChecks(u8 *pos, u32 size, u16 **off, u16 **off2)
{
    //Look for signature checks
    const u8 pattern[] = {0xC0, 0x1C, 0x76, 0xE7},
             pattern2[] = {0xB5, 0x22, 0x4D, 0x0C};

    *off = (u16 *)memsearch(pos, pattern, size, 4);
    *off2 = (u16 *)(memsearch(pos, pattern2, size, 4) - 1);
}

void *getReboot(u8 *pos, u32 size, u32 process9MemAddr, u32 *fOpenOffset)
{
    //Look for FIRM reboot code
    const u8 pattern[] = {0xDE, 0x1F, 0x8D, 0xE2};

    u8 *off = memsearch(pos, pattern, size, 4) - 0x10;

    //Firmlaunch function offset - offset in BLX opcode (A4-16 - ARM DDI 0100E) + 1
    *fOpenOffset = (u32)(off + 9 - (-((*(u32 *)off & 0x00FFFFFF) << 2) & (0xFFFFFF << 2)) - pos + process9MemAddr);

    return off;
}

u16 *getFirmWrite(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    u8 *const off = memsearch(pos, "exe:", size, 4);
    const u8 pattern[] = {0x00, 0x28, 0x01, 0xDA};

    return (u16 *)memsearch(off - 0x100, pattern, 0x100, 4);
}

u16 *getFirmWriteSafe(u8 *pos, u32 size)
{
    //Look for FIRM writing code
    const u8 pattern[] = {0x04, 0x1E, 0x1D, 0xDB};

    return (u16 *)memsearch(pos, pattern, size, 4);
}

u32 getLoader(u8 *pos, u32 *loaderSize)
{
    u8 *off = pos;
    u32 size;

    while(1)
    {
        size = *(u32 *)(off + 0x104) * 0x200;
        if(*(u32 *)(off + 0x200) == 0x64616F6C) break;
        off += size;
    }

    *loaderSize = size;

    return (u32)(off - pos);
}

u32 *getSvcAndExceptions(u8 *pos, u32 size, u32 **exceptionsPage)
{
    const u8 pattern[] = {0x00, 0xB0, 0x9C, 0xE5}; //cpsid aif
    
    *exceptionsPage = (u32 *)(memsearch(pos, pattern, size, 4) - 0x2C);

    u32 svcOffset = (-(((*exceptionsPage)[2] & 0xFFFFFF) << 2) & (0xFFFFFF << 2)) - 8; //Branch offset + 8 for prefetch
    u32 *svcTable = (u32 *)(pos + *(u32 *)(pos + 0xFFFF0008 - svcOffset - 0xFFF00000 + 8) - 0xFFF00000); //SVC handler address
    while(*svcTable) svcTable++; //Look for SVC0 (NULL)

    return svcTable;
}