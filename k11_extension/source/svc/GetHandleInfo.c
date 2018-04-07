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

Result GetHandleInfoHook(s64 *out, Handle handle, u32 type)
{
    if(type == 0x10000) // KDebug and KProcess: get context ID
    {
        KProcessHwInfo *hwInfo;
        KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
        KAutoObject *obj;
        if(handle == CUR_PROCESS_HANDLE)
        {
            obj = (KAutoObject *)(currentCoreContext->objectContext.currentProcess);
            KAutoObject__AddReference(obj);
        }
        else
            obj = KProcessHandleTable__ToKAutoObject(handleTable, handle);
        
        if(obj == NULL)
            return 0xD8E007F7;

        if(strcmp(classNameOfAutoObject(obj), "KDebug") == 0)
            hwInfo = hwInfoOfProcess(((KDebug *)obj)->owner);
        else if(strcmp(classNameOfAutoObject(obj), "KProcess") == 0)
            hwInfo = hwInfoOfProcess((KProcess *)obj);
        else
            hwInfo = NULL;

        *out = hwInfo != NULL ? KPROCESSHWINFO_GET_RVALUE(hwInfo, contextId) : -1;

        obj->vtable->DecrementReferenceCount(obj);
        return 0;
    }
    else
        return GetHandleInfo(out, handle, type);
}
