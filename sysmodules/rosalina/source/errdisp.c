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
#include "fmt.h"
#include "ifile.h"

static inline void assertSuccess(Result res)
{
    if(R_FAILED(res))
        svcBreak(USERBREAK_PANIC);
}

static MyThread errDispThread;
static u8 ALIGN(8) errDispThreadStack[0x2000];

static char userString[0x100 + 1] = {0};

MyThread *errDispCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&errDispThread, errDispThreadMain, errDispThreadStack, 0x2000, 0x18, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &errDispThread;
}

static inline u32 ERRF_DisplayRegisterValue(u32 posX, u32 posY, const char *name, u32 value)
{
    return Draw_DrawFormattedString(posX, posY, COLOR_WHITE, "%-9s %08x", name, value);
}

static inline int ERRF_FormatRegisterValue(char *out, const char *name, u32 value)
{
    return sprintf(out, "%-9s %08x", name, value);
}

static int ERRF_FormatError(char *out, ERRF_FatalErrInfo *info)
{
    char *outStart = out;
    static const char *types[] = {
        "generic", "corrupted", "card removed", "exception", "result failure", "logged", "invalid"
    };

    static const char *exceptionTypes[] = {
        "prefetch abort", "data abort", "undefined instruction", "VFP", "invalid"
    };

    const char *type = (u32)info->type > (u32)ERRF_ERRTYPE_LOGGED ? types[6] : types[(u32)info->type];

    if(info->type == ERRF_ERRTYPE_EXCEPTION)
    {
        const char *exceptionType = (u32)info->data.exception_data.excep.type > (u32)ERRF_EXCEPTION_VFP ?
                                    exceptionTypes[4] : exceptionTypes[(u32)info->data.exception_data.excep.type];

        out += sprintf(out, "Error type:       exception (%s)\n", exceptionType);
    }
    else
        out += sprintf(out, "Error type:       %s\n", type);

    if(info->type != ERRF_ERRTYPE_CARD_REMOVED)
    {
        Handle processHandle;
        Result res;

        out += sprintf(out, "\nProcess ID:       %u\n", info->procId);

        res = svcOpenProcess(&processHandle, info->procId);
        if(R_SUCCEEDED(res))
        {
            u64 titleId;
            char name[9] = { 0 };
            svcGetProcessInfo((s64 *)name, processHandle, 0x10000);
            svcGetProcessInfo((s64 *)&titleId, processHandle, 0x10001);
            svcCloseHandle(processHandle);
            out += sprintf(out, "Process name:     %s\n", name);
            out += sprintf(out, "Process title ID: 0x%016llx\n", titleId);
        }

        out += sprintf(out, "\n");
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
            out += ERRF_FormatRegisterValue(out, registerNames[i], regs[i]);
            if(i != 16)
            {
                out += sprintf(out, "          ");
                out += ERRF_FormatRegisterValue(out,  registerNames[i + 1], i == 16 ? regs[20] : regs[i + 1]);
                out += sprintf(out, "\n");
            }
        }

        if(info->data.exception_data.excep.type == ERRF_EXCEPTION_PREFETCH_ABORT
        || info->data.exception_data.excep.type == ERRF_EXCEPTION_DATA_ABORT)
        {
            out += sprintf(out, "          ");
            out += ERRF_FormatRegisterValue(out, "far", info->data.exception_data.excep.far);
            out += sprintf(out, "\n");
            out += ERRF_FormatRegisterValue(out, "fsr", info->data.exception_data.excep.fsr);
        }

        else if(info->data.exception_data.excep.type == ERRF_EXCEPTION_VFP)
        {
            out += sprintf(out, "          ");
            out += ERRF_FormatRegisterValue(out, "fpexc", info->data.exception_data.excep.fpexc);
            out += sprintf(out, "\n");
            out += ERRF_FormatRegisterValue(out, "fpinst", info->data.exception_data.excep.fpinst);
            out += sprintf(out, "          ");
            out += ERRF_FormatRegisterValue(out, "fpinst2", info->data.exception_data.excep.fpinst2);
            out += sprintf(out, "\n");
        }

        out += sprintf(out, "\n");
    }
    else if(info->type != ERRF_ERRTYPE_CARD_REMOVED)
    {
        if(info->type != ERRF_ERRTYPE_FAILURE)
            out += sprintf(out, "Address:          0x%08x\n", info->pcAddr);

        out += sprintf(out, "Error code:       0x%08x\n", info->resCode);
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

    if(desc[0] != 0)
        out += sprintf(out, "\n%s\n", desc);
    out += sprintf(out, "\n");
    return out - outStart;
}


static void ERRF_DisplayError(ERRF_FatalErrInfo *info)
{
    Draw_Lock();

    u32 posY = Draw_DrawString(10, 10, COLOR_RED, userString[0] == 0 ? "An error occurred (ErrDisp)" : userString);
    char buf[0x400];
    
    ERRF_FormatError(buf, info);
    posY = posY < 30 ? 30 : posY;

    posY = Draw_DrawString(10, posY, COLOR_WHITE, buf);
    posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Press any button to reboot.");

    Draw_FlushFramebuffer();
    Draw_Unlock();
}

static Result ERRF_SaveErrorToFile(ERRF_FatalErrInfo *info)
{
    char buf[0x400];

    FS_ArchiveID archiveId;
    s64 out;
    u64 size, total;
    bool isSdMode;
    int n = 0;
    Result res = 0;
    IFile file;

    n = ERRF_FormatError(buf, info);
    n += sprintf(buf + n, "-------------------------------------\n\n");

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    res = IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/errdisp.txt"), FS_OPEN_WRITE | FS_OPEN_CREATE);
    
    if(R_FAILED(res))
        return res;
    
    res = IFile_GetSize(&file, &size);
    if(R_FAILED(res))
    {
        IFile_Close(&file);
        return res;
    }

    file.pos = size;

    res = IFile_Write(&file, &total, buf, (u32)n, 0);
    IFile_Close(&file);

    return res;
}

static void ERRF_HandleCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();

    switch(cmdbuf[0] >> 16)
    {
        case 1: // Throw
        {
            ERRF_FatalErrInfo *info = (ERRF_FatalErrInfo *)(cmdbuf + 1);
            if(info->type != ERRF_ERRTYPE_LOGGED || info->procId == 0 || R_FAILED(ERRF_SaveErrorToFile(info)))
            {
                menuEnter();

                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();

                ERRF_DisplayError(info);

                /*
                If we ever wanted to return:
                Draw_Unlock();
                menuLeave();

                but we don't
                */
                waitInput();
                svcKernelSetState(7);
                __builtin_unreachable();
            }

            cmdbuf[0] = 0x10040;
            cmdbuf[1] = 0;
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
