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

#include "svc/CopyHandle.h"
#include "memory.h"

Result CopyHandle(Handle *outHandle, Handle outProcessHandle, Handle inHandle, Handle inProcessHandle)
{
    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
    KProcess *inProcess, *outProcess;
    Result res;

    if(inProcessHandle == CUR_PROCESS_HANDLE)
    {
        inProcess = currentCoreContext->objectContext.currentProcess;
        KAutoObject__AddReference((KAutoObject *)inProcess);
    }
    else
        inProcess = KProcessHandleTable__ToKProcess(handleTable, inProcessHandle);

    if(inProcess == NULL)
        return 0xD8E007F7; // invalid handle

    if(outProcessHandle == CUR_PROCESS_HANDLE)
    {
        outProcess = currentCoreContext->objectContext.currentProcess;
        KAutoObject__AddReference((KAutoObject *)outProcess);
    }
    else
        outProcess = KProcessHandleTable__ToKProcess(handleTable, outProcessHandle);

    if(outProcess == NULL)
    {
        ((KAutoObject *)inProcess)->vtable->DecrementReferenceCount((KAutoObject *)inProcess);
        return 0xD8E007F7; // invalid handle
    }

    KAutoObject *obj = KProcessHandleTable__ToKAutoObject(handleTableOfProcess(inProcess), inHandle);
    if(obj == NULL)
    {
        ((KAutoObject *)inProcess)->vtable->DecrementReferenceCount((KAutoObject *)inProcess);
        ((KAutoObject *)outProcess)->vtable->DecrementReferenceCount((KAutoObject *)outProcess);
        return 0xD8E007F7; // invalid handle
    }

    res = createHandleForProcess(outHandle, outProcess, obj);

    obj->vtable->DecrementReferenceCount(obj);
    ((KAutoObject *)inProcess)->vtable->DecrementReferenceCount((KAutoObject *)inProcess);
    ((KAutoObject *)outProcess)->vtable->DecrementReferenceCount((KAutoObject *)outProcess);

    return res;
}
