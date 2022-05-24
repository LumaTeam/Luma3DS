/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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

#include "synchronization.h"
#include "utils.h"
#include "kernel.h"
#include "globals.h"

extern SGI0Handler_t SGI0Handler;

void executeFunctionOnCores(SGI0Handler_t handler, u8 targetList, u8 targetListFilter)
{
    u32 coreID = getCurrentCoreID();
    SGI0Handler = handler;

    if(targetListFilter == 0 && (targetListFilter & (1 << coreID)) != 0)
        __enable_irq(); // make sure interrupts aren't masked
    MPCORE_GID_SGI = (targetListFilter << 24) | (targetList << 16) | 0;
}

void KScheduler__TriggerCrossCoreInterrupt(KScheduler *this)
{
    this->triggerCrossCoreInterrupt = false;
    for(s16 i = 0; i < (s16)getNumberOfCores(); i++)
    {
        if(this->coreNumber != i)
            MPCORE_GID_SGI = (1 << (16 + i)) | 8;
    }
}

void KThread__DebugReschedule(KThread *this, bool lock)
{
    KRecursiveLock__Lock(criticalSectionLock);

    u32 oldSchedulingMask = this->schedulingMask;
    if(lock) // the original k11 function discards the other flags
        this->schedulingMask |= 0x80;
    else
        this->schedulingMask &= ~0x80;

    KScheduler__AdjustThread(currentCoreContext->objectContext.currentScheduler, this, oldSchedulingMask);

    KRecursiveLock__Unlock(criticalSectionLock);
}

static void rosalinaLockThread(KThread *thread)
{
    KThread *syncThread = synchronizationMutex->owner;

    if(syncThread == NULL || syncThread != thread)
        rosalinaRescheduleThread(thread, true);
}

void rosalinaRescheduleThread(KThread *thread, bool lock)
{
    KRecursiveLock__Lock(criticalSectionLock);

    u32 oldSchedulingMask = thread->schedulingMask;
    if(lock)
        thread->schedulingMask |= 0x40;
    else
        thread->schedulingMask &= ~0x40;

    if (oldSchedulingMask != thread->schedulingMask)
        KScheduler__AdjustThread(currentCoreContext->objectContext.currentScheduler, thread, oldSchedulingMask);

    KRecursiveLock__Unlock(criticalSectionLock);
}

bool rosalinaThreadLockPredicate(KThread *thread, u32 mask)
{
    KProcess *process = thread->ownerProcess;
    if(process == NULL || idOfProcess(process) < nbSection0Modules)
        return false;

    u64 titleId = codeSetOfProcess(process)->titleId;
    u32 highTitleId = (u32)(titleId >> 32), lowTitleId = (u32)(titleId & ~0xF0000001); // clear N3DS and SAFE_FIRM bits

    if (mask & 1)
    {
        if (highTitleId != 0x00040130) // non-sysmodules
            return true;
        else
            return lowTitleId == 0x1A02 || lowTitleId == 0x2702; // dsp, csnd
    }
    if (mask & 2)
    {
        if (highTitleId != 0x00040130) // non-sysmodules
            return false;
        return lowTitleId == 0x1C02; // gsp
    }
    if (mask & 4)
    {
        if (highTitleId != 0x00040130) // non-sysmodules
            return false;
        return lowTitleId == 0x1D02 || lowTitleId == 0x3302;
    }

    return false;
}

void rosalinaLockThreads(u32 mask)
{
    bool currentThreadsFound = false;

    KRecursiveLock__Lock(criticalSectionLock);
    for(KLinkedListNode *node = threadList->list.nodes.first; node != (KLinkedListNode *)&threadList->list.nodes; node = node->next)
    {
        KThread *thread = (KThread *)node->key;
        if(!rosalinaThreadLockPredicate(thread, mask))
            continue;
        if(thread == coreCtxs[thread->coreId].objectContext.currentThread)
            currentThreadsFound = true;
        else
            rosalinaLockThread(thread);
    }

    if(currentThreadsFound)
    {
        for(KLinkedListNode *node = threadList->list.nodes.first; node != (KLinkedListNode *)&threadList->list.nodes; node = node->next)
        {
            KThread *thread = (KThread *)node->key;
            if(!rosalinaThreadLockPredicate(thread, mask))
                continue;
            if(!(thread->schedulingMask & 0x40))
            {
                rosalinaLockThread(thread);
                KRecursiveLock__Lock(criticalSectionLock);
                if(thread->coreId != getCurrentCoreID())
                {
                    u32 cpsr = __get_cpsr();
                    __disable_irq();
                    coreCtxs[thread->coreId].objectContext.currentScheduler->triggerCrossCoreInterrupt = true;
                    currentCoreContext->objectContext.currentScheduler->triggerCrossCoreInterrupt = true;
                    __set_cpsr_cx(cpsr);
                }
                KRecursiveLock__Unlock(criticalSectionLock);
            }
        }
        KScheduler__TriggerCrossCoreInterrupt(currentCoreContext->objectContext.currentScheduler);
    }
    KRecursiveLock__Unlock(criticalSectionLock);
}

void rosalinaUnlockThreads(u32 mask)
{
    for(KLinkedListNode *node = threadList->list.nodes.first; node != (KLinkedListNode *)&threadList->list.nodes; node = node->next)
    {
        KThread *thread = (KThread *)node->key;

        if((thread->schedulingMask & 0xF) == 2) // thread is terminating
            continue;

        if((thread->schedulingMask & 0x40) && rosalinaThreadLockPredicate(thread, mask))
            rosalinaRescheduleThread(thread, false);
    }
}
