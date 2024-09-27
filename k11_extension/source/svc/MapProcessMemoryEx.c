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

#include "svc/MapProcessMemoryEx.h"

Result  MapProcessMemoryEx(Handle dstProcessHandle, u32 vaDst, Handle srcProcessHandle, u32 vaSrc, u32 size, MapExFlags flags)
{
    Result          res = 0;
    u32             sizeInPage = size >> 12;
    KLinkedList     list;
    KProcess        *srcProcess;
    KProcess        *dstProcess;
    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);

    if (dstProcessHandle == CUR_PROCESS_HANDLE)
    {
        dstProcess = currentCoreContext->objectContext.currentProcess;
        KAutoObject__AddReference((KAutoObject *)dstProcess);
    }
    else
        dstProcess = KProcessHandleTable__ToKProcess(handleTable, dstProcessHandle);

    if (dstProcess == NULL)
        return 0xD8E007F7;

    if (srcProcessHandle == CUR_PROCESS_HANDLE)
    {
        srcProcess = currentCoreContext->objectContext.currentProcess;
        KAutoObject__AddReference((KAutoObject *)srcProcess);
    }
    else
        srcProcess = KProcessHandleTable__ToKProcess(handleTable, srcProcessHandle);

    if (srcProcess == NULL)
    {
        res =  0xD8E007F7;
        goto exit1;
    }

    KLinkedList__Initialize(&list);

    res = KProcessHwInfo__GetListOfKBlockInfoForVA(hwInfoOfProcess(srcProcess), &list, vaSrc, sizeInPage);

    if (res >= 0)
    {
        // Check if the destination address is free and large enough
        res = KProcessHwInfo__CheckVaState(hwInfoOfProcess(dstProcess), vaDst, size, 0, 0);
        if (res == 0)
            res = KProcessHwInfo__MapListOfKBlockInfo(hwInfoOfProcess(dstProcess), vaDst, &list, (flags & MAPEXFLAGS_PRIVATE) ? 0xBB05 : 0x5806, MEMPERM_RW | 0x18, 0);
    }

    KLinkedList_KBlockInfo__Clear(&list);

    ((KAutoObject *)srcProcess)->vtable->DecrementReferenceCount((KAutoObject *)srcProcess);

exit1:
    ((KAutoObject *)dstProcess)->vtable->DecrementReferenceCount((KAutoObject *)dstProcess);

    invalidateEntireInstructionCache();
    flushEntireDataCache();

    return res;
}
