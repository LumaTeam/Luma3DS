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

#include "ipc.h"
#include "memory.h"

static SessionInfo sessionInfos[MAX_SESSION] = { {NULL} };
static u32 nbActiveSessions = 0;
static KRecursiveLock sessionInfosLock = { NULL };

KRecursiveLock processLangemuLock;
LangemuAttributes processLangemuAttributes[0x40];

static void *customSessionVtable[0x10] = { NULL }; // should be enough

static u32 SessionInfo_FindClosestSlot(KSession *session)
{
    if(nbActiveSessions == 0 || session <= sessionInfos[0].session)
        return 0;
    else if(session > sessionInfos[nbActiveSessions - 1].session)
        return nbActiveSessions;

    u32 a = 0, b = nbActiveSessions - 1, m;

    do
    {
        m = (a + b) / 2;
        if(sessionInfos[m].session < session)
            a = m;
        else if(sessionInfos[m].session > session)
            b = m;
        else
            return m;
    }
    while(b - a > 1);

    return b;
}

SessionInfo *SessionInfo_Lookup(KSession *session)
{
    KRecursiveLock__Lock(criticalSectionLock);
    KRecursiveLock__Lock(&sessionInfosLock);

    SessionInfo *ret;
    u32 id = SessionInfo_FindClosestSlot(session);
    if(id == nbActiveSessions)
        ret = NULL;
    else
        ret = (void **)(sessionInfos[id].session->autoObject.vtable) == customSessionVtable ? &sessionInfos[id] : NULL;
    
    KRecursiveLock__Unlock(&sessionInfosLock);
    KRecursiveLock__Unlock(criticalSectionLock);

    return ret;
}

SessionInfo *SessionInfo_FindFirst(const char *name)
{
    KRecursiveLock__Lock(criticalSectionLock);
    KRecursiveLock__Lock(&sessionInfosLock);

    SessionInfo *ret;
    u32 id;
    for(id = 0; id < nbActiveSessions && strncmp(sessionInfos[id].name, name, 12) != 0; id++);
    if(id == nbActiveSessions)
        ret = NULL;
    else
        ret = (void **)(sessionInfos[id].session->autoObject.vtable) == customSessionVtable ? &sessionInfos[id] : NULL;

    KRecursiveLock__Unlock(&sessionInfosLock);
    KRecursiveLock__Unlock(criticalSectionLock);

    return ret;
}

void SessionInfo_Add(KSession *session, const char *name)
{
    KAutoObject__AddReference(&session->autoObject);
    SessionInfo_ChangeVtable(session);
    session->autoObject.vtable->DecrementReferenceCount(&session->autoObject);

    KRecursiveLock__Lock(criticalSectionLock);
    KRecursiveLock__Lock(&sessionInfosLock);

    if(nbActiveSessions == MAX_SESSION)
    {
        KRecursiveLock__Unlock(&sessionInfosLock);
        KRecursiveLock__Unlock(criticalSectionLock);
        return;
    }

    u32 id = SessionInfo_FindClosestSlot(session);

    if(id != nbActiveSessions && sessionInfos[id].session == session)
    {
        KRecursiveLock__Unlock(&sessionInfosLock);
        KRecursiveLock__Unlock(criticalSectionLock);
        return;
    }

    for(u32 i = nbActiveSessions; i > id && i != 0; i--)
        sessionInfos[i] = sessionInfos[i - 1];

    nbActiveSessions++;

    sessionInfos[id].session = session;
    strncpy(sessionInfos[id].name, name, 12);

    KRecursiveLock__Unlock(&sessionInfosLock);
    KRecursiveLock__Unlock(criticalSectionLock);
}

void SessionInfo_Remove(KSession *session)
{
    KRecursiveLock__Lock(criticalSectionLock);
    KRecursiveLock__Lock(&sessionInfosLock);

    if(nbActiveSessions == MAX_SESSION)
    {
        KRecursiveLock__Unlock(&sessionInfosLock);
        KRecursiveLock__Unlock(criticalSectionLock);
        return;
    }

    u32 id = SessionInfo_FindClosestSlot(session);

    if(id == nbActiveSessions)
    {
        KRecursiveLock__Unlock(&sessionInfosLock);
        KRecursiveLock__Unlock(criticalSectionLock);
        return;
    }

    for(u32 i = id; i < nbActiveSessions - 1; i++)
        sessionInfos[i] = sessionInfos[i + 1];

    memset(&sessionInfos[--nbActiveSessions], 0, sizeof(SessionInfo));

    KRecursiveLock__Unlock(&sessionInfosLock);
    KRecursiveLock__Unlock(criticalSectionLock);
}

static void (*KSession__dtor_orig)(KAutoObject *this);
static void KSession__dtor_hook(KAutoObject *this)
{
    KSession__dtor_orig(this);
    SessionInfo_Remove((KSession *)this);
}

void SessionInfo_ChangeVtable(KSession *session)
{
    if(customSessionVtable[2] == NULL)
    {
        memcpy(customSessionVtable, session->autoObject.vtable, 0x40);
        KSession__dtor_orig = session->autoObject.vtable->dtor;
        customSessionVtable[2] = (void *)KSession__dtor_hook;
    }
    session->autoObject.vtable = (Vtable__KAutoObject *)customSessionVtable;
}

bool doLangEmu(Result *res, u32 *cmdbuf)
{
    KRecursiveLock__Lock(criticalSectionLock);
    KRecursiveLock__Lock(&processLangemuLock);

    u64 titleId = codeSetOfProcess(currentCoreContext->objectContext.currentProcess)->titleId;
    LangemuAttributes *attribs = NULL;
    bool skip = true;

    *res = 0;
    for(u32 i = 0; i < 0x40; i++)
    {
        if(processLangemuAttributes[i].titleId == titleId)
            attribs = &processLangemuAttributes[i];
    }

    if(attribs == NULL)
    {
        KRecursiveLock__Unlock(&processLangemuLock);
        KRecursiveLock__Unlock(criticalSectionLock);
        return false;
    }

    if((cmdbuf[0] == 0x20000 || cmdbuf[0] == 0x4060000 || cmdbuf[0] == 0x8160000) && (attribs->mask & 1)) // SecureInfoGetRegion
    {
        cmdbuf[0] = (cmdbuf[0] & 0xFFFF0000) | 0x80;
        cmdbuf[1] = 0;
        cmdbuf[2] = attribs->region;
    }
    else if(cmdbuf[1] == 1 && cmdbuf[2] == 0xA0002 && cmdbuf[3] == 0x1C && (attribs->mask & 2))
    {
        cmdbuf[0] = (cmdbuf[0] & 0xFFFF0000) | 0x40;
        cmdbuf[1] = 0;
        *(u8 *)cmdbuf[4] = attribs->language;
    }
    else if(cmdbuf[1] == 4 && cmdbuf[2] == 0xB0000 && cmdbuf[3] == 0x4C && (attribs->mask & 0xC))
    {
        u8 *ptr = (u8 *)cmdbuf[4];
        if(attribs->mask & 4)
            ptr[3] = attribs->country;
        if(attribs->mask & 8)
            ptr[2] = attribs->state;

        ptr[0] = ptr[1] = 0;
    }
    else
        skip = false;

    KRecursiveLock__Unlock(&processLangemuLock);
    KRecursiveLock__Unlock(criticalSectionLock);
    return skip;
}

Result doPublishToProcessHook(Handle handle, u32 *cmdbuf)
{
    Result res = 0;
    u32 pid;
    bool terminateRosalina = cmdbuf[1] == 0x100 && cmdbuf[2] == 0; // cmdbuf[2] to check for well-formed requests
    u32 savedCmdbuf[4];
    memcpy(savedCmdbuf, cmdbuf, 16);
    
    if(!terminateRosalina || GetProcessId(&pid, cmdbuf[3]) != 0)
        terminateRosalina = false;
    else
    {
        KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
        KProcess *process = KProcessHandleTable__ToKProcess(handleTable, cmdbuf[3]);
        if((strcmp(codeSetOfProcess(process)->processName, "socket") == 0 && (rosalinaState & 2)) ||
            strcmp(codeSetOfProcess(process)->processName, "pxi") == 0)
            terminateRosalina = true;
        else
            terminateRosalina = false;
        ((KAutoObject *)process)->vtable->DecrementReferenceCount((KAutoObject *)process);
    }
    
    if(terminateRosalina && nbSection0Modules == 6)
    {
        Handle rosalinaProcessHandle;
        res = OpenProcess(&rosalinaProcessHandle, 5);
        if(res == 0)
        {
            cmdbuf[0] = cmdbuf[0];
            cmdbuf[1] = 0x100;
            cmdbuf[2] = 0;
            cmdbuf[3] = rosalinaProcessHandle;

            res = SendSyncRequest(handle);
            CloseHandle(rosalinaProcessHandle);
            memcpy(cmdbuf, savedCmdbuf, 16);
        }
    }

    return SendSyncRequest(handle);
}

bool doErrfThrowHook(u32 *cmdbuf)
{
    // If fatalErrorInfo->type is "card removed" or "logged", returning from ERRF:Throw is a no-op
    // for the SDK function

    // r6 (arm) or r4 (thumb) is copied into cmdbuf[1..31]
    u32 *r0_to_r7_r12_usr = (u32 *)((u8 *)currentCoreContext->objectContext.currentThread->endOfThreadContext - 0x110);
    u32 spsr = *(u32 *)((u8 *)currentCoreContext->objectContext.currentThread->endOfThreadContext - 0xCC);
    u8 *srcerrbuf = (u8 *)r0_to_r7_r12_usr[(spsr & 0x20) ? 4 : 6];
    const char *pname = codeSetOfProcess(currentCoreContext->objectContext.currentProcess)->processName;

    static const struct
    {
        const char *name;
        Result errCode;
    } errorCodesToIgnore[] =
    {
        /*
            If you're getting this error, you have broken your head-tracking hardware,
            and should uncomment the following line:
        */
        //{ "qtm", (Result)0xF96183FE },

        { "", 0 }, // impossible case to ensure the array has at least 1 element 
    };

    for(u32 i = 0; i < sizeof(errorCodesToIgnore) / sizeof(errorCodesToIgnore[0]); i++)
    {
        if(strcmp(pname, errorCodesToIgnore[i].name) == 0 && (Result)cmdbuf[2] == errorCodesToIgnore[i].errCode)
        {
            srcerrbuf[0] = 5;
            cmdbuf[0] = 0x10040;
            cmdbuf[1] = 0;
            return true;
        }
    }

    return false;
}
