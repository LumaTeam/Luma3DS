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

#include "svc/GetThreadInfo.h"
#include "memory.h"

Result GetThreadInfoHook(s64 *out, Handle threadHandle, u32 type)
{
    Result res = 0;

    if(type == 0x10000) // Get TLS
    {
        KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
        KThread *thread;

        if(threadHandle == CUR_THREAD_HANDLE)
        {
            thread = currentCoreContext->objectContext.currentThread;
            KAutoObject__AddReference(&thread->syncObject.autoObject);
        }
        else
            thread = KProcessHandleTable__ToKThread(handleTable, threadHandle);

        if(thread == NULL)
            return 0xD8E007F7; // invalid handle

        *out = (s64)(u64)(u32)thread->threadLocalStorage;

        KAutoObject *obj = (KAutoObject *)thread;
        obj->vtable->DecrementReferenceCount(obj);
    }

    else
        res = GetThreadInfo(out, threadHandle, type);

    return res;
}
