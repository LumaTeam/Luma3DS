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

#include "svc/ConnectToPort.h"
#include "ipc.h"

Result ConnectToPortHook(Handle *out, const char *name)
{
    char portName[12] = {0};
    Result res = 0;
    if(name != NULL)
    {
        s32 nb = usrToKernelStrncpy(portName, name, 12);
        if(nb < 0)
            return 0xD9001814;
        else if(nb == 12 && portName[11] != 0)
            return 0xE0E0181E;
    }
    res = ConnectToPort(out, name);
    if(res != 0)
    {
        // Prior to 11.0 kernel didn't zero-initialize output handles, and thus
        // you could accidentaly close things like the KAddressArbiter handle by mistake...
        *out = 0;
        return res;
    }

    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
    KClientSession *clientSession = (KClientSession *)KProcessHandleTable__ToKAutoObject(handleTable, *out);
    if(clientSession != NULL)
    {
        SessionInfo_Add(clientSession->parentSession, portName);
        clientSession->syncObject.autoObject.vtable->DecrementReferenceCount(&clientSession->syncObject.autoObject);
    }

    return res;
}
