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

#include <3ds.h>
#include "errdisp.h"
#include "draw.h"
#include "menu.h"
#include "memory.h"

static inline void assertSuccess(Result res)
{
    if(R_FAILED(res))
        svcBreak(USERBREAK_PANIC);
}

static MyThread errDispThread;
static u8 ALIGN(8) errDispThreadStack[THREAD_STACK_SIZE];

static char userString[0x100 + 1] = {0};

MyThread *errDispCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&errDispThread, errDispThreadMain, errDispThreadStack, THREAD_STACK_SIZE, 0x18, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &errDispThread;
}

static inline u32 ERRF_DisplayRegisterValue(u32 posX, u32 posY, const char *name, u32 value)
{
    return Draw_DrawFormattedString(posX, posY, COLOR_WHITE, "%-9s %08x", name, value);
}

void ERRF_DisplayError(ERRF_FatalErrInfo *info)
{
    Draw_Lock();

    u32 posY = Draw_DrawString(10, 10, COLOR_RED, userString[0] == 0 ? "An error occurred (ErrDisp)" : userString);

    static const char *types[] = {
        "generic", "corrupted", "card removed", "exception", "result failure", "logged", "invalid"
    };

    static const char *exceptionTypes[] = {
        "prefetch abort", "data abort", "undefined instruction", "VFP", "invalid"
    };

    const char *type = (u32)info->type > (u32)ERRF_ERRTYPE_LOGGED ? types[6] : types[(u32)info->type];
    posY = posY < 30 ? 30 : posY;

    if(info->type == ERRF_ERRTYPE_EXCEPTION)
    {
        const char *exceptionType = (u32)info->data.exception_data.excep.type > (u32)ERRF_EXCEPTION_VFP ?
                                    exceptionTypes[4] : exceptionTypes[(u32)info->data.exception_data.excep.type];

        Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Error type:       exception (%s)", exceptionType);
    }
    else
        Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Error type:       %s", type);

    if(info->type != ERRF_ERRTYPE_CARD_REMOVED)
    {
        Handle processHandle;
        Result res;

        posY += SPACING_Y;
        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Process ID:       %u", info->procId);

        res = svcOpenProcess(&processHandle, info->procId);
        if(R_SUCCEEDED(res))
        {
            u64 titleId;
            char name[9] = { 0 };
            svcGetProcessInfo((s64 *)name, processHandle, 0x10000);
            svcGetProcessInfo((s64 *)&titleId, processHandle, 0x10001);
            svcCloseHandle(processHandle);
            posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Process name:     %s", name);
            posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Process title ID: 0x%016llx", titleId);
        }

        posY += SPACING_Y;
    }

    if(info->type == ERRF_ERRTYPE_EXCEPTION)
    {
        static const char *registerNames[] = {
            "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
            "sp", "lr", "pc", "cpsr"
        };

        u32 *regs = (u32 *)(&info->data.exception_data.regs);
        for(u32 i = 0; i < 17; i += 2)
        {
            posY = ERRF_DisplayRegisterValue(10, posY + SPACING_Y, registerNames[i], regs[i]);

            if(i != 16)
                ERRF_DisplayRegisterValue(10 + 28 * SPACING_X, posY, registerNames[i + 1], i == 16 ? regs[20] : regs[i + 1]);
        }

        if(info->data.exception_data.excep.type == ERRF_EXCEPTION_PREFETCH_ABORT
        || info->data.exception_data.excep.type == ERRF_EXCEPTION_DATA_ABORT)
        {
            ERRF_DisplayRegisterValue(10 + 28 * SPACING_X, posY, "far", info->data.exception_data.excep.far);
            posY = ERRF_DisplayRegisterValue(10, posY + SPACING_Y, "fsr", info->data.exception_data.excep.fsr);
        }

        else if(info->data.exception_data.excep.type == ERRF_EXCEPTION_VFP)
        {
            ERRF_DisplayRegisterValue(10 + 28 * SPACING_X, posY, "fpexc", info->data.exception_data.excep.fpexc);
            posY = ERRF_DisplayRegisterValue(10, posY + SPACING_Y, "fpinst", info->data.exception_data.excep.fpinst);
            ERRF_DisplayRegisterValue(10 + 28 * SPACING_X, posY, "fpinst2", info->data.exception_data.excep.fpinst2);
        }
    }

    else if(info->type != ERRF_ERRTYPE_CARD_REMOVED)
    {
        if(info->type != ERRF_ERRTYPE_FAILURE)
            posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Address:          0x%08x", info->pcAddr);

        posY = Draw_DrawFormattedString(10, posY + SPACING_Y, COLOR_WHITE, "Error code:       0x%08x", info->resCode);
    }

    const char *desc;
    switch(info->type)
    {
        case ERRF_ERRTYPE_CARD_REMOVED:
            desc = "The card was removed.";
            break;
        case ERRF_ERRTYPE_MEM_CORRUPT:
            desc = "The System Memory has been damaged.";
            break;
        case ERRF_ERRTYPE_FAILURE:
            info->data.failure_mesg[0x60] = 0; // make sure the last byte in the IPC buffer is NULL
            desc = info->data.failure_mesg;
            break;
        default:
            desc = "";
            break;
    }

    posY += SPACING_Y;
    if(desc[0] != 0)
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, desc) + SPACING_Y;
    posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Press any button to reboot");

    Draw_FlushFramebuffer();
    Draw_Unlock();
}

void ERRF_HandleCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();

    switch(cmdbuf[0] >> 16)
    {
        case 1: // Throw
        {
            ERRF_FatalErrInfo *info = (ERRF_FatalErrInfo *)(cmdbuf + 1);
            menuEnter();

            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();

            ERRF_DisplayError(info);

            /*
            If we ever wanted to return:
            draw_unlock();
            menuLeave();

            cmdbuf[0] = 0x10040;
            cmdbuf[1] = 0;

            but we don't
            */
            waitInput();
            svcKernelSetState(7);
            __builtin_unreachable();
            break;
        }

        case 2: // SetUserString
        {
            if(cmdbuf[0] != 0x20042 || (cmdbuf[2] & 0x3C0F) != 2)
            {
                cmdbuf[0] = 0x40;
                cmdbuf[1] = 0xD9001830;
            }
            else
            {
                cmdbuf[0] = 0x20040;
                u32 sz = cmdbuf[1] <= 0x100 ? sz : 0x100;
                memcpy(userString, cmdbuf + 3, sz);
                userString[sz] = 0;
            }
            break;
        }
    }
}

void errDispThreadMain(void)
{
    Handle handles[2];
    Handle serverHandle, clientHandle, sessionHandle = 0;

    u32 replyTarget = 0;
    s32 index;

    Result res;
    u32 *cmdbuf = getThreadCommandBuffer();

    assertSuccess(svcCreatePort(&serverHandle, &clientHandle, "err:f", 1));

    do
    {
        handles[0] = serverHandle;
        handles[1] = sessionHandle;

        if(replyTarget == 0) // k11
            cmdbuf[0] = 0xFFFF0000;
        res = svcReplyAndReceive(&index, handles, sessionHandle == 0 ? 1 : 2, replyTarget);

        if(R_FAILED(res))
        {
            if((u32)res == 0xC920181A) // session closed by remote
            {
                svcCloseHandle(sessionHandle);
                sessionHandle = 0;
                replyTarget = 0;
            }

            else
                svcBreak(USERBREAK_PANIC);
        }

        else
        {
            if(index == 0)
            {
                Handle session;
                assertSuccess(svcAcceptSession(&session, serverHandle));

                if(sessionHandle == 0)
                    sessionHandle = session;
                else
                    svcCloseHandle(session);
            }
            else
            {
                ERRF_HandleCommands();
                replyTarget = sessionHandle;
            }
        }
    }
    while(!terminationRequest);

    svcCloseHandle(sessionHandle);
    svcCloseHandle(clientHandle);
    svcCloseHandle(serverHandle);
}
