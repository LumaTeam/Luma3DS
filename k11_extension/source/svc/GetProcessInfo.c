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

#include "svc/GetProcessInfo.h"
#include "memory.h"

Result GetProcessInfoHook(s64 *out, Handle processHandle, u32 type)
{
    Result res = 0;

    if(type >= 0x10000)
    {
        KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
        KProcess *process;
        if(processHandle == CUR_PROCESS_HANDLE)
        {
            process = currentCoreContext->objectContext.currentProcess;
            KAutoObject__AddReference((KAutoObject *)process);
        }
        else
            process = KProcessHandleTable__ToKProcess(handleTable, processHandle);

        if(process == NULL)
            return 0xD8E007F7; // invalid handle

        switch(type)
        {
            case 0x10000:
                memcpy(out, codeSetOfProcess(process)->processName, 8);
                break;
            case 0x10001:
                *(u64 *)out = codeSetOfProcess(process)->titleId;
                break;
            case 0x10002:
                *out = codeSetOfProcess(process)->nbTextPages << 12;
                break;
            case 0x10003:
                *out = codeSetOfProcess(process)->nbRodataPages << 12;
                break;
            case 0x10004:
                *out = codeSetOfProcess(process)->nbRwPages << 12;
                break;
            case 0x10005:
                *out = (s64)(u64)(u32)codeSetOfProcess(process)->textSection.section.loadAddress;
                break;
            case 0x10006:
                *out = (s64)(u64)(u32)codeSetOfProcess(process)->rodataSection.section.loadAddress;
                break;
            case 0x10007:
                *out = (s64)(u64)(u32)codeSetOfProcess(process)->dataSection.section.loadAddress;
                break;
            case 0x10008:
            {
                KProcessHwInfo *hwInfo = hwInfoOfProcess(process);
                u32 ttb = KPROCESSHWINFO_GET_RVALUE(hwInfo, translationTableBase);
                *out = ttb & ~((1 << (14 - TTBCR)) - 1);
                break;
            }
            default:
                res = 0xD8E007ED; // invalid enum value
                break;
        }

        ((KAutoObject *)process)->vtable->DecrementReferenceCount((KAutoObject *)process);
    }

    else
        res = GetProcessInfo(out, processHandle, type);

    return res;
}
