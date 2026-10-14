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

#include "svc/TerminateProcess.h"

#define PLG_TERMINATE_SIGNAL_TIMEOUT 15000000000LL

Result TerminateProcessHook(Handle processHandle)
{
    KProcessHandleTable *handleTable = handleTableOfProcess(currentCoreContext->objectContext.currentProcess);
    KProcess *process;

    if (processHandle == CUR_PROCESS_HANDLE)
    {
        process = currentCoreContext->objectContext.currentProcess;
        KAutoObject__AddReference((KAutoObject *)process);
    }
    else
        process = KProcessHandleTable__ToKProcess(handleTable, processHandle);

    if (process != NULL)
    {
        u32 *flags = KPROCESS_GET_PTR(process, customFlags);

        // A process ended by svcTerminateProcess (by PM when it doesn't exit within the timeout
        // after notification 0x100) never reaches the svcExitProcess hook. Without the exit
        // event, Rosalina would keep the plugin memory block and its handle to the process.
        if (*flags & SignalOnExit)
        {
            u32 status = PLG_GetStatus();

            if (status == PLG_CFG_RUNNING || status == PLG_CFG_INHOME)
            {
                *flags &= ~SignalOnExit;
                PLG_SignalEventWithTimeout(PLG_CFG_EXIT_EVENT, PLG_TERMINATE_SIGNAL_TIMEOUT);
            }
        }

        ((KAutoObject *)process)->vtable->DecrementReferenceCount((KAutoObject *)process);
    }

    return ((Result (*)(Handle))officialSVCs[0x76])(processHandle);
}
