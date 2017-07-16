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

#include "svc/SendSyncRequest.h"
#include "memory.h"
#include "ipc.h"

Result SendSyncRequestHook(Handle handle)
{
    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
    KClientSession *clientSession = (KClientSession *)KProcessHandleTable__ToKAutoObject(handleTable, handle);

    u32 *cmdbuf = (u32 *)((u8 *)currentCoreContext->objectContext.currentThread->threadLocalStorage + 0x80);
    bool skip = false;
    Result res = 0;

     // not the exact same test but it should work
    bool isValidClientSession = clientSession != NULL && strcmp(classNameOfAutoObject(&clientSession->syncObject.autoObject), "KClientSession") == 0;

    if(isValidClientSession)
    {
        switch (cmdbuf[0])
        {
            case 0x10042:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && kernelVersion >= SYSTEM_VERSION(2, 39, 4) && strcmp(info->name, "srv:pm") == 0)
                {
                    res = doPublishToProcessHook(handle, cmdbuf);
                    skip = true;
                }
                else if(info != NULL && strcmp(info->name, "ndm:u") == 0 && hasStartedRosalinaNetworkFuncsOnce)
                {
                    cmdbuf[0] = 0x10040;
                    cmdbuf[1] = 0;
                    skip = true;
                }

                break;
            }

            case 0x10082:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && (strcmp(info->name, "cfg:u") == 0 || strcmp(info->name, "cfg:s") == 0 || strcmp(info->name, "cfg:i") == 0)) // GetConfigInfoBlk2
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x10800:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && strcmp(info->name, "err:f") == 0) // Throw
                    skip = doErrfThrowHook(cmdbuf);

                break;
            }

            case 0x20000:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && (strcmp(info->name, "cfg:u") == 0 || strcmp(info->name, "cfg:s") == 0 || strcmp(info->name, "cfg:i") == 0)) // SecureInfoGetRegion
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x20002:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && strcmp(info->name, "ndm:u") == 0 && hasStartedRosalinaNetworkFuncsOnce)
                {
                    cmdbuf[0] = 0x20040;
                    cmdbuf[1] = 0;
                    skip = true;
                }

                break;
            }

            case 0x50100:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && (strcmp(info->name, "srv:") == 0 || (kernelVersion < SYSTEM_VERSION(2, 39, 4) && strcmp(info->name, "srv:pm") == 0)))
                {
                    char name[9] = { 0 };
                    memcpy(name, cmdbuf + 1, 8);

                    skip = true;
                    res = SendSyncRequest(handle);
                    if(res == 0)
                    {
                        KClientSession *outClientSession;

                        outClientSession = (KClientSession *)KProcessHandleTable__ToKAutoObject(handleTable, (Handle)cmdbuf[3]);
                        if(outClientSession != NULL)
                        {
                            if(strcmp(classNameOfAutoObject(&outClientSession->syncObject.autoObject), "KClientSession") == 0)
                                SessionInfo_Add(outClientSession->parentSession, name);
                            outClientSession->syncObject.autoObject.vtable->DecrementReferenceCount(&outClientSession->syncObject.autoObject);
                        }
                    }
                }

                break;
            }

            case 0x80040:
            {
                if(!hasStartedRosalinaNetworkFuncsOnce)
                    break;
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                skip = info != NULL && strcmp(info->name, "ndm:u") == 0; // SuspendScheduler
                if(skip)
                    cmdbuf[1] = 0;
                break;
            }

            case 0x90000:
            {
                if(!hasStartedRosalinaNetworkFuncsOnce)
                    break;
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && strcmp(info->name, "ndm:u") == 0) // ResumeScheduler
                {
                    cmdbuf[0] = 0x90040;
                    cmdbuf[1] = 0;
                    skip = true;
                }
                break;
            }

            case 0x4010042:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && kernelVersion < SYSTEM_VERSION(2, 39, 4) && strcmp(info->name, "srv:pm") == 0)
                {
                    res = doPublishToProcessHook(handle, cmdbuf);
                    skip = true;
                }

                break;
            }

            case 0x4010082:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && (strcmp(info->name, "cfg:s") == 0 || strcmp(info->name, "cfg:i") == 0)) // GetConfigInfoBlk4
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x4020082:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && (strcmp(info->name, "cfg:s") == 0 || strcmp(info->name, "cfg:i") == 0)) // GetConfigInfoBlk8
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x8010082:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && (strcmp(info->name, "cfg:s") == 0 || strcmp(info->name, "cfg:i") == 0)) // GetConfigInfoBlk4
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x8020082:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession);
                if(info != NULL && strcmp(info->name, "cfg:i") == 0) // GetConfigInfoBlk8
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x4060000:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession); // SecureInfoGetRegion
                if(info != NULL && (strcmp(info->name, "cfg:s") == 0 || strcmp(info->name, "cfg:i") == 0))
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }

            case 0x8160000:
            {
                SessionInfo *info = SessionInfo_Lookup(clientSession->parentSession); // SecureInfoGetRegion
                if(info != NULL && strcmp(info->name, "cfg:i") == 0)
                    skip = doLangEmu(&res, cmdbuf);

                break;
            }
        }
    }

    if(clientSession != NULL)
        clientSession->syncObject.autoObject.vtable->DecrementReferenceCount(&clientSession->syncObject.autoObject);

    res = skip ? res : SendSyncRequest(handle);

    return res;
}
