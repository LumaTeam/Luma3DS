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
#include <string.h>

#include "svc/ControlService.h"
#include "ipc.h"

Result ControlService(ServiceOp op, u32 varg1, u32 varg2)
{
    Result res = 0;
    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);

    switch(op)
    {
        case SERVICEOP_GET_NAME:
        {
            KSession *session = NULL;
            SessionInfo *info = NULL;
            KAutoObject *obj = KProcessHandleTable__ToKAutoObject(handleTable, (Handle)varg2);
            if(obj == NULL)
                return 0xD8E007F7; // invalid handle
            else
            {
                // not the exact same tests but it should work
                if(strcmp(classNameOfAutoObject(obj), "KServerSession") == 0)
                    session = ((KServerSession *)obj)->parentSession;
                else if(strcmp(classNameOfAutoObject(obj), "KClientSession") == 0)
                    session = ((KClientSession *)obj)->parentSession;
            }

            if(session != NULL)
                info = SessionInfo_Lookup(session);

            if(info == NULL)
                res = 0xD8E007F7;
            else
            {
                // names are limited to 11 characters (for ports)
                // kernelToUsrStrncpy doesn't clear trailing bytes
                char name[12] = { 0 };
                strncpy(name, info->name, 12);
                res = kernelToUsrMemcpy8((void *)varg1, name, strlen(name) + 1) ? 0 : 0xE0E01BF5;
            }

            obj->vtable->DecrementReferenceCount(obj);
            return res;
        }

        case SERVICEOP_STEAL_CLIENT_SESSION:
        {
            char name[12] = { 0 };
            SessionInfo *info = NULL;
            s32 nb = usrToKernelStrncpy(name, (const char *)varg2, 12);
            if(nb < 0)
                return 0xD9001814;
            else if(nb == 12 && name[11] != 0)
                return 0xE0E0181E;

            info = SessionInfo_FindFirst(name);

            if(info == NULL)
                return 0x9401BFE; // timeout (the wanted service is likely not initalized)
            else
            {
                Handle out;
                res = createHandleForThisProcess(&out, &info->session->clientSession.syncObject.autoObject);
                return (res != 0) ? res : (kernelToUsrMemcpy32((u32 *)varg1, (u32 *)&out, 4) ? 0 : (Result)0xE0E01BF5);
            }
        }

        default:
            return 0xF8C007F4;
    }
}
