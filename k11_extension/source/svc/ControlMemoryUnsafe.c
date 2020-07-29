/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2018 Aurora Wright, TuxSH
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

#include "globals.h"
#include "memory.h"
#include "svc/ControlMemoryUnsafe.h"

Result  ControlMemoryUnsafe(u32 *out, u32 addr0, u32 size, MemOp op, MemPerm perm)
{
    Result          res = 0;
    KProcess        *currentProcess = currentCoreContext->objectContext.currentProcess;
    KProcessHwInfo  *hwInfo = hwInfoOfProcess(currentProcess);

    KAutoObject__AddReference((KAutoObject *)currentProcess);

    size = size >> 12 << 12;
    switch (op & MEMOP_OP_MASK)
    {
        case MEMOP_FREE:
        {
            res = doControlMemory(hwInfo, addr0, size >> 12, 0, 0, 0, 0, 0);
            break;
        }
        case MEMOP_COMMIT:
        {
            u32 pAddr = 0;
            u32 state = 0xBB05;
            u32 region = op & MEMOP_REGION_MASK;

            perm = (perm & 7) | 0x18;
            if (op & MEMOP_LINEAR)
            {
                void *kvAddr = kAlloc(fcramDescriptor, size >> 12, 0, region);

                if (!kvAddr)
                {
                    res = 0xD86007F3;
                    break;
                }

                memset(kvAddr, 0, size >> 2);
                flushDataCacheRange(kvAddr, size);
                pAddr = (u32)kvAddr + 0x40000000;
                state = 0x3907;
            }

            res  = doControlMemory(hwInfo, addr0, size >> 12, pAddr, state, perm, 0, region);
            if (res >= 0 && out)
                *out = addr0;
            break;
        }

        default:
            res = 0xE0E01BEE;
            break;
    }

    ((KAutoObject *)currentProcess)->vtable->DecrementReferenceCount((KAutoObject *)currentProcess);

    return res;
}
