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

#include <3ds.h>
#include "errdisp.h"
#include "draw.h"
#include "menu.h"
#include "memory.h"
#include "fmt.h"
#include "ifile.h"

extern Handle preTerminationEvent;

static MyThread errDispThread;
static u8 CTR_ALIGN(8) errDispThreadStack[0xD00];

static char userString[0x100 + 1] = {0};
static char staticBuf[sizeof(userString)] = {0};

MyThread *errDispCreateThread(void)
{
    if(R_FAILED(MyThread_Create(&errDispThread, errDispThreadMain, errDispThreadStack, 0xD00, 55, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &errDispThread;
}

static inline u32 ERRF_DisplayRegisterValue(u32 posX, u32 posY, const char *name, u32 value)
{
    return Draw_DrawFormattedString(posX, posY, COLOR_WHITE, "%-9s %08lx", name, value);
}

static inline int ERRF_FormatRegisterValue(char *out, const char *name, u32 value)
{
    return sprintf(out, "%-9s %08lx", name, value);
}

static inline void ERRF_PreprocessErrInfo(ERRF_FatalErrInfo *info, u32 *in)
{
    memcpy(info, in, sizeof(ERRF_FatalErrInfo));

    Result res = info->resCode;
    u32 level = R_LEVEL(res);
    u32 module = R_MODULE(res);
    u32 summary = R_SUMMARY(res);
    u32 desc = R_DESCRIPTION(res);

    // ErrDisp has special handling where it expects two errors in a row
    // for "card removed", but it's fine if we don't.

    if (module != RM_FS)
        return;

    switch (desc)
    {
        // All kinds of "slot1 stuff removed" error ranges
        case 0x08C ... 0x095:
        case 0x096 ... 0x0A9:
        case 0x122 ... 0x12B:
        case 0x12C ... 0x13F:
            info->type = ERRF_ERRTYPE_CARD_REMOVED;
            module = RM_CARD;
            break;
        // SDMC removed error range
        case 0x0AA ... 0x0B3:
            info->type = ERRF_ERRTYPE_CARD_REMOVED;
            module = RM_SDMC;
            break;
        // Say sike right now
        case 0x399:
            info->type = ERRF_ERRTYPE_NAND_DAMAGED;
            break;
    }

    info->resCode = MAKERESULT(level, summary, module, desc);
}

static int ERRF_FormatRegisterDump(char *out, const ERRF_ExceptionData *exceptionData)
{
    char *out0 = out;
    static const char *registerNames[] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12",
        "sp", "lr", "pc", "cpsr"
    };

    u32 *regs = (u32 *)(&exceptionData->regs);
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

    if(exceptionData->excep.type == ERRF_EXCEPTION_PREFETCH_ABORT
    || exceptionData->excep.type == ERRF_EXCEPTION_DATA_ABORT)
    {
        out += sprintf(out, "          ");
        out += ERRF_FormatRegisterValue(out, "far", exceptionData->excep.far);
        out += sprintf(out, "\n");
        out += ERRF_FormatRegisterValue(out, "fsr", exceptionData->excep.fsr);
    }

    else if(exceptionData->excep.type == ERRF_EXCEPTION_VFP)
    {
        out += sprintf(out, "          ");
        out += ERRF_FormatRegisterValue(out, "fpexc", exceptionData->excep.fpexc);
        out += sprintf(out, "\n");
        out += ERRF_FormatRegisterValue(out, "fpinst", exceptionData->excep.fpinst);
        out += sprintf(out, "          ");
        out += ERRF_FormatRegisterValue(out, "fpinst2", exceptionData->excep.fpinst2);
    }
    return (int)(out - out0);
}

static int ERRF_FormatGenericInfo(char *out, const ERRF_FatalErrInfo *info)
{
    static const char *types[] = {
        "generic", "corrupted", "card removed", "exception", "result failure", "generic (log only)", "invalid"
    };

    static const char *exceptionTypes[] = {
        "prefetch abort", "data abort", "undefined instruction", "VFP", "invalid"
    };

    const char *type = (u32)info->type > (u32)ERRF_ERRTYPE_LOG_ONLY ? types[6] : types[(u32)info->type];

    char *out0 = out;
    Handle processHandle;
    Result res;

    if(info->type == ERRF_ERRTYPE_EXCEPTION)
    {
        const char *exceptionType = (u32) info->data.exception_data.excep.type > (u32)ERRF_EXCEPTION_VFP ?
                                    exceptionTypes[4] : exceptionTypes[(u32)info->data.exception_data.excep.type];

        out += sprintf(out, "Error type:       exception (%s)\n", exceptionType);
    }
    else
        out += sprintf(out, "Error type:       %s\n", type);

    out += sprintf(out, "\nProcess ID:       %lu\n", info->procId);

    res = svcOpenProcess(&processHandle, info->procId);
    if(R_SUCCEEDED(res))
    {
        u64 titleId;
        char name[9] = { 0 };
        svcGetProcessInfo((s64 *)name, processHandle, 0x10000);
        svcGetProcessInfo((s64 *)&titleId, processHandle, 0x10001);
        svcCloseHandle(processHandle);
        out += sprintf(out, "Process name:     %s\n", name);
        out += sprintf(out, "Process title ID: %016llx\n", titleId);
    }

    out += sprintf(out, "\n");

    return (int)(out - out0);
}

static int ERRF_FormatError(char *out, const ERRF_FatalErrInfo *info, bool isLogFile)
{
    char *outStart = out;

    if (isLogFile)
    {
        char dateTimeStr[32];
        u64 timeNow = osGetTime();
        u64 timeAtBoot = timeNow - (1000 * svcGetSystemTick() / SYSCLOCK_ARM11);
        dateTimeToString(dateTimeStr, timeNow, false);
        out += sprintf(out, "Reported on:      %s\n", dateTimeStr);
        dateTimeToString(dateTimeStr, timeAtBoot, false);
        out += sprintf(out, "System booted on: %s\n\n", dateTimeStr);

    }
    switch (info->type)
    {
        case ERRF_ERRTYPE_NAND_DAMAGED:
            out += sprintf(out, "The NAND chip has been damaged.\n");
            break;
        case ERRF_ERRTYPE_CARD_REMOVED:
        {
            const char *medium = R_MODULE(info->resCode) == RM_SDMC ? "SD card" : "cartridge";
            out += sprintf(out, "The %s was removed.\n", medium);
            break;
        }
        case ERRF_ERRTYPE_GENERIC:
        case ERRF_ERRTYPE_LOG_ONLY:
            out += ERRF_FormatGenericInfo(out, info);
            out += sprintf(out, "Address:          0x%08lx\n", info->pcAddr);
            out += sprintf(out, "Error code:       0x%08lx\n", info->resCode);
            break;
        case ERRF_ERRTYPE_EXCEPTION:
            out += ERRF_FormatGenericInfo(out, info);
            out += ERRF_FormatRegisterDump(out, &info->data.exception_data);
            out += sprintf(out, "\n");
            break;
        case ERRF_ERRTYPE_FAILURE:
            out += ERRF_FormatGenericInfo(out, info);
            out += sprintf(out, "Error code:       0x%08lx\n", info->resCode);
            out += sprintf(out, "Reason:           %.96s\n", info->data.failure_mesg);
            break;
        default:
            out += sprintf(out, "Invalid fatal error data.\n");
    }

    // We might not always have enough space to display this on screen, so keep it to the log file
    if (isLogFile && userString[0] != '\0')
        out += sprintf(out, "\n%.256s\n", userString);

    out += sprintf(out, "\n");
    return out - outStart;
}


static void ERRF_DisplayError(ERRF_FatalErrInfo *info)
{
    Draw_Lock();

    u32 posY = Draw_DrawString(10, 10, COLOR_RED, "An error occurred (ErrDisp)");
    char buf[0x400];

    ERRF_FormatError(buf, info, false);
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

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    // Bail out early if we know we're unable to write anything, and do not log "SD/cartridge removed" stuff
    if (info->type == ERRF_ERRTYPE_CARD_REMOVED)
        return 0;
    else if (!isSdMode && info->type == ERRF_ERRTYPE_NAND_DAMAGED)
        return info->resCode;

    n = ERRF_FormatError(buf, info, true);
    n += sprintf(buf + n, "-------------------------------------\n\n");

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

void ERRF_HandleCommands(void)
{
    u32 *cmdbuf = getThreadCommandBuffer();
    ERRF_FatalErrInfo info;

    switch(cmdbuf[0] >> 16)
    {
        case 1: // Throw
        {
            ERRF_PreprocessErrInfo(&info, (cmdbuf + 1));
            ERRF_SaveErrorToFile(&info);
            if(!menuShouldExit && info.type != ERRF_ERRTYPE_LOG_ONLY)
            {
                menuEnter();

                Draw_Lock();
                Draw_ClearFramebuffer();
                Draw_FlushFramebuffer();

                ERRF_DisplayError(&info);

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

            cmdbuf[0] = IPC_MakeHeader(1, 1, 0);
            cmdbuf[1] = 0;
            break;
        }

        case 2: // SetUserString
        {
            if(cmdbuf[0] != IPC_MakeHeader(2, 1, 2) || (cmdbuf[2] & 0x3C0F) != 2)
            {
                cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
                cmdbuf[1] = 0xD9001830;
            }
            else
            {
                // Official kernel doesn't copy, but this means it could be overwritten by a rogue command
                u32 sz = cmdbuf[2] >> 14;
                memcpy(userString, (const char *)cmdbuf[3], sz);
                userString[sz] = 0;

                cmdbuf[0] = IPC_MakeHeader(2, 1, 0);
                cmdbuf[1] = 0;
            }
            break;
        }
        default: // error
        {
            cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
            cmdbuf[1] = 0xD900182F;
            break;
        }
    }
}

void errDispThreadMain(void)
{
    Handle handles[3];
    Handle serverHandle, clientHandle, sessionHandle = 0;

    u32 replyTarget = 0;
    s32 index;

    Result res;
    u32 *cmdbuf = getThreadCommandBuffer();
    u32 *sbuf = getThreadStaticBuffers();

    sbuf[0] = IPC_Desc_StaticBuffer(0x100, 0);
    sbuf[1] = (u32)staticBuf;

    assertSuccess(svcCreatePort(&serverHandle, &clientHandle, "err:f", 1));

    do
    {
        handles[0] = preTerminationEvent;
        handles[1] = serverHandle;
        handles[2] = sessionHandle;

        if(replyTarget == 0) // k11
            cmdbuf[0] = 0xFFFF0000;
        res = svcReplyAndReceive(&index, handles, 1 + (sessionHandle == 0 ? 1 : 2), replyTarget);

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
            if (index == 0)
            {
                break;
            }
            else if(index == 1)
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
    while(!preTerminationRequested);

    svcCloseHandle(sessionHandle);
    svcCloseHandle(clientHandle);
    svcCloseHandle(serverHandle);
}