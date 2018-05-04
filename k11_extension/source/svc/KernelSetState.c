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

#include "svc/KernelSetState.h"
#include "synchronization.h"
#include "ipc.h"
#include "memory.h"

#define MAX_DEBUG 3

static u32 nbEnabled = 0;
static u32 maskedPids[MAX_DEBUG];
static u32 masks[MAX_DEBUG][8] = {0};

bool shouldSignalSyscallDebugEvent(KProcess *process, u8 svcId)
{
    u32 pid = idOfProcess(process);
    u32 id;
    for(id = 0; id < nbEnabled && maskedPids[id] != pid; id++);
    if(id == MAX_DEBUG)
        return false;
    else
        return ((masks[id][svcId / 32] >> (31 - (svcId % 32))) & 1) != 0;
}

Result SetSyscallDebugEventMask(u32 pid, bool enable, const u32 *mask)
{
    static KRecursiveLock syscallDebugEventMaskLock = { NULL };

    u32 tmpMask[8];
    if(enable && nbEnabled == MAX_DEBUG)
        return 0xC86018FF; // Out of resource (255)

    if(enable && !usrToKernelMemcpy8(&tmpMask, mask, 32))
        return 0xE0E01BF5;

    KRecursiveLock__Lock(criticalSectionLock);
    KRecursiveLock__Lock(&syscallDebugEventMaskLock);

    if(enable)
    {
        maskedPids[nbEnabled] = pid;
        memcpy(&masks[nbEnabled++], tmpMask, 32);
    }
    else
    {
        u32 id;
        for(id = 0; id < nbEnabled && maskedPids[id] != pid; id++);
        if(id == nbEnabled)
        {
                KRecursiveLock__Unlock(&syscallDebugEventMaskLock);
                KRecursiveLock__Unlock(criticalSectionLock);
                return 0xE0E01BFD; // out of range (it's not fully technically correct but meh)
        }

        for(u32 i = id; i < nbEnabled - 1; i++)
        {
            maskedPids[i] = maskedPids[i + 1];
            memcpy(&masks[i], &masks[i + 1], 32);
        }
        maskedPids[--nbEnabled] = 0;
        memset(&masks[nbEnabled], 0, 32);
    }

    KRecursiveLock__Unlock(&syscallDebugEventMaskLock);
    KRecursiveLock__Unlock(criticalSectionLock);
    return 0;
}

Result KernelSetStateHook(u32 type, u32 varg1, u32 varg2, u32 varg3)
{
    Result res = 0;

    switch(type)
    {
        case 0x10000:
        {
            do
            {
                __ldrex((s32 *)&rosalinaState);
            }
            while(__strex((s32 *)&rosalinaState, (s32)(rosalinaState ^ varg1)));

            if(rosalinaState & 2)
                hasStartedRosalinaNetworkFuncsOnce = true;

            if(rosalinaState & 1)
                rosalinaLockAllThreads();
            else if(varg1 & 1)
                rosalinaUnlockAllThreads();

            break;
        }
        case 0x10001:
        {
            KRecursiveLock__Lock(criticalSectionLock);
            KRecursiveLock__Lock(&processLangemuLock);

            u32 i;
            for(i = 0; i < 0x40 && processLangemuAttributes[i].titleId != 0ULL; i++);
            if(i < 0x40)
            {
                processLangemuAttributes[i].titleId = ((u64)varg3 << 32) | (u32)varg2;
                processLangemuAttributes[i].state = (u8)(varg1 >> 24);
                processLangemuAttributes[i].country = (u8)(varg1 >> 16);
                processLangemuAttributes[i].language = (u8)(varg1 >> 8);
                processLangemuAttributes[i].region = (u8)((varg1 >> 4) & 0xf);
                processLangemuAttributes[i].mask = (u8)(varg1 & 0xf);
            }
            else
                res = 0xD8609013;

            KRecursiveLock__Unlock(&processLangemuLock);
            KRecursiveLock__Unlock(criticalSectionLock);
            break;
        }
        case 0x10002:
        {
            res = SetSyscallDebugEventMask(varg1, (bool)varg2, (const u32 *)varg3);
            break;
        }

        default:
        {
            res = KernelSetState(type, varg1, varg2, varg3);
            break;
        }
    }

    return res;
}
