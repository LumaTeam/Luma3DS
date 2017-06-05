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

/* This file was entirely written by fincs */

#include <3ds.h>
#include "3dsx.h"
#include "memory.h"

#define Log_PrintP(...) ((void)0)

#define MAXRELOCS 512
static _3DSX_Reloc s_relocBuf[MAXRELOCS];
u32 ldrArgvBuf[ARGVBUF_SIZE/4];

#define SEC_ASSERT(x) do { if (!(x)) { Log_PrintP("Assertion failed: %s", #x); return false; } } while (0)

typedef struct
{
    void* segPtrs[3]; // code, rodata & data
    u32 segAddrs[3];
    u32 segSizes[3];
} _3DSX_LoadInfo;

static inline u32 TranslateAddr(u32 off, _3DSX_LoadInfo* d, u32* offsets)
{
    if (off < offsets[0])
        return d->segAddrs[0] + off;
    if (off < offsets[1])
        return d->segAddrs[1] + off - offsets[0];
    return d->segAddrs[2] + off - offsets[1];
}

u32 IFile_Read2(IFile *file, void* buffer, u32 size, u32 offset)
{
    Result res;
    u64 total = 0;
    file->pos = offset;
    res = IFile_Read(file, &total, buffer, size);
    return R_SUCCEEDED(res) ? total : 0;
}

bool Ldr_Get3dsxSize(u32* pSize, IFile *file)
{
    _3DSX_Header hdr;

    if (IFile_Read2(file, &hdr, sizeof(hdr), 0) != sizeof(hdr))
    {
        Log_PrintP("Cannot read 3DSX header");
        return false;
    }

    if (hdr.magic != _3DSX_MAGIC)
    {
        Log_PrintP("Not a valid 3DSX file");
        return false;
    }

    u32 segSizes[3];
    segSizes[0] = (hdr.codeSegSize+0xFFF) &~ 0xFFF;
    segSizes[1] = (hdr.rodataSegSize+0xFFF) &~ 0xFFF;
    segSizes[2] = (hdr.dataSegSize+0xFFF) &~ 0xFFF;
    SEC_ASSERT(segSizes[0] >= hdr.codeSegSize);
    SEC_ASSERT(segSizes[1] >= hdr.rodataSegSize);
    SEC_ASSERT(segSizes[2] >= hdr.dataSegSize);

    // TODO: Check int overflow
    *pSize = segSizes[0] + segSizes[1] + segSizes[2] + 0x1000; // One extra page reserved for settings/etc

    return true;
}

static inline u32 min(u32 a, u32 b)
{
    return a < b ? a : b;
}

Handle Ldr_CodesetFrom3dsx(const char* name, u32* codePages, u32 baseAddr, IFile *file, u64 tid)
{
    u32 i,j,k,m;
    Result res;
    _3DSX_Header hdr;
    IFile_Read2(file, &hdr, sizeof(hdr), 0);

    _3DSX_LoadInfo d;
    d.segSizes[0] = (hdr.codeSegSize+0xFFF) &~ 0xFFF;
    d.segSizes[1] = (hdr.rodataSegSize+0xFFF) &~ 0xFFF;
    d.segSizes[2] = (hdr.dataSegSize+0xFFF) &~ 0xFFF;
    d.segPtrs[0] = codePages;
    d.segPtrs[1] = (char*)d.segPtrs[0] + d.segSizes[0];
    d.segPtrs[2] = (char*)d.segPtrs[1] + d.segSizes[1];
    d.segAddrs[0] = baseAddr;
    d.segAddrs[1] = d.segAddrs[0] + d.segSizes[0];
    d.segAddrs[2] = d.segAddrs[1] + d.segSizes[1];

    u32 offsets[2] = { d.segSizes[0], d.segSizes[0] + d.segSizes[1] };
    u32* segLimit = d.segPtrs[2] + d.segSizes[2];

    u32 readOffset = hdr.headerSize;

    u32 nRelocTables = hdr.relocHdrSize/4;
    SEC_ASSERT((3*4*nRelocTables) <= 0x1000);
    u32* extraPage = (u32*)((char*)d.segPtrs[2] + d.segSizes[2]);
    u32 extraPageAddr = d.segAddrs[2] + d.segSizes[2];

    // Read the relocation headers
    for (i = 0; i < 3; i ++)
    {
        if (IFile_Read2(file, &extraPage[i*nRelocTables], hdr.relocHdrSize, readOffset) != hdr.relocHdrSize)
        {
            Log_PrintP("Cannot read relheader %d", i);
            return 0;
        }
        readOffset += hdr.relocHdrSize;
    }

    // Read the code segment
    if (IFile_Read2(file, d.segPtrs[0], hdr.codeSegSize, readOffset) != hdr.codeSegSize)
    {
        Log_PrintP("Cannot read code segment");
        return 0;
    }
    readOffset += hdr.codeSegSize;

    // Read the rodata segment
    if (IFile_Read2(file, d.segPtrs[1], hdr.rodataSegSize, readOffset) != hdr.rodataSegSize)
    {
        Log_PrintP("Cannot read rodata segment");
        return 0;
    }
    readOffset += hdr.rodataSegSize;

    // Read the data segment
    u32 dataLoadSegSize = hdr.dataSegSize - hdr.bssSize;
    if (IFile_Read2(file, d.segPtrs[2], dataLoadSegSize, readOffset) != dataLoadSegSize)
    {
        Log_PrintP("Cannot read data segment");
        return 0;
    }
    readOffset += dataLoadSegSize;

    // Relocate the segments
    for (i = 0; i < 3; i ++)
    {
        for (j = 0; j < nRelocTables; j ++)
        {
            int nRelocs = extraPage[i*nRelocTables + j];
            if (j >= (sizeof(_3DSX_RelocHdr)/4))
            {
                // Not using this header
                readOffset += nRelocs;
                continue;
            }

            u32* pos = (u32*)d.segPtrs[i];
            u32* endPos = pos + (d.segSizes[i]/4);
            SEC_ASSERT(endPos <= segLimit);

            while (nRelocs)
            {
                u32 toDo = nRelocs > MAXRELOCS ? MAXRELOCS : nRelocs;
                nRelocs -= toDo;

                u32 readSize = toDo*sizeof(_3DSX_Reloc);
                if (IFile_Read2(file, s_relocBuf, readSize, readOffset) != readSize)
                {
                    Log_PrintP("Cannot read reloc table (%d,%d)", i, j);
                    return 0;
                }
                readOffset += readSize;

                for (k = 0; k < toDo && pos < endPos; k ++)
                {
                    pos += s_relocBuf[k].skip;
                    u32 nPatches = s_relocBuf[k].patch;
                    for (m = 0; m < nPatches && pos < endPos; m ++)
                    {
                        u32 inAddr = baseAddr + 4*(pos - codePages);
                        u32 origData = *pos;
                        u32 subType = origData >> (32-4);
                        u32 addr = TranslateAddr(origData &~ 0xF0000000, &d, offsets);
                        //Log_PrintP("%08lX<-%08lX", inAddr, addr);
                        switch (j)
                        {
                            case 0:
                            {
                                if (subType != 0)
                                {
                                    Log_PrintP("Unsupported absolute reloc subtype (%lu)", subType);
                                    return 0;
                                }
                                *pos = addr;
                                break;
                            }
                            case 1:
                            {
                                u32 data = addr - inAddr;
                                switch (subType)
                                {
                                    case 0: *pos = data;            break; // 32-bit signed offset
                                    case 1: *pos = data &~ BIT(31); break; // 31-bit signed offset
                                    default:
                                        Log_PrintP("Unsupported relative reloc subtype (%lu)", subType);
                                        return 0;
                                }
                                break;
                            }
                        }
                        pos++;
                    }
                }
            }
        }
    }

    // Detect and fill _prm structure
    PrmStruct* pst = (PrmStruct*) &codePages[1];
    if (pst->magic == _PRM_MAGIC)
    {
        memset(extraPage, 0, 0x1000);
        memcpy(extraPage, ldrArgvBuf, sizeof(ldrArgvBuf));
        pst->pSrvOverride = extraPageAddr + 0xFFC;
        pst->pArgList = extraPageAddr;
        pst->runFlags |= RUNFLAG_APTCHAINLOAD;
        s64 dummy;
        bool isN3DS = svcGetSystemInfo(&dummy, 0x10001, 0) == 0;
        if (isN3DS)
        {
            pst->heapSize = 48*1024*1024;
            pst->linearHeapSize = 64*1024*1024;
        } else
        {
            pst->heapSize = 24*1024*1024;
            pst->linearHeapSize = 32*1024*1024;
        }
    }

    // Create the codeset
    CodeSetInfo csinfo;
    memset(&csinfo, 0, sizeof(csinfo));
    strncpy((char*)csinfo.name, name, 8);
    csinfo.program_id      = tid;
    csinfo.text_addr       = d.segAddrs[0];
    csinfo.text_size       = d.segSizes[0] >> 12;
    csinfo.ro_addr         = d.segAddrs[1];
    csinfo.ro_size         = d.segSizes[1] >> 12;
    csinfo.rw_addr         = d.segAddrs[2];
    csinfo.rw_size         = (d.segSizes[2] >> 12) + 1; // One extra page reserved for settings/etc
    csinfo.text_size_total = csinfo.text_size;
    csinfo.ro_size_total   = csinfo.ro_size;
    csinfo.rw_size_total   = csinfo.rw_size;
    Handle hCodeset = 0;
    res = svcCreateCodeSet(&hCodeset, &csinfo, d.segPtrs[0], d.segPtrs[1], d.segPtrs[2]);
    if (res)
    {
        Log_PrintP("svcCreateCodeSet: %08lX", res);
        return 0;
    }

    return hCodeset;
}
