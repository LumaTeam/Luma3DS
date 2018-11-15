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
#include "svc/MapProcessMemoryEx.h"

Result UnmapProcessMemoryEx(Handle processHandle, void *dst, u32 size)
{
    Result          res = 0;
    u32             sizeInPage = size >> 12;
    KLinkedList     list;
    KProcess        *process;
    KProcessHwInfo  *hwInfo;
    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);

    if (processHandle == CUR_PROCESS_HANDLE)
    {
        process = currentCoreContext->objectContext.currentProcess;
        KAutoObject__AddReference((KAutoObject *)process);
    }
    else
        process = KProcessHandleTable__ToKProcess(handleTable, processHandle);

    if (process == NULL)
        return 0xD8E007F7;

    hwInfo = hwInfoOfProcess(process);

    KLinkedList__Initialize(&list);

    res = KProcessHwInfo__GetListOfKBlockInfoForVA(hwInfo, &list, (u32)dst, sizeInPage);

    if (res >= 0)
    {
        // Check for dst address to be in the right state (0x5806 as we set it with svcMapProcessMemoryEx)
        res = KProcessHwInfo__CheckVaState(hwInfo, (u32)dst, size, 0x5806, 0);
        if (res == 0)
            res = KProcessHwInfo__MapListOfKBlockInfo(hwInfo, (u32)dst, &list, 0, 0, 0);
    }

    KLinkedList_KBlockInfo__Clear(&list);


    ((KAutoObject *)process)->vtable->DecrementReferenceCount((KAutoObject *)process);

    invalidateEntireInstructionCache();
    flushEntireDataCache();

    return res;
}
