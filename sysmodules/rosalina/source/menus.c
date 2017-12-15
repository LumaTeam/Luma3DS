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
#include <3ds/os.h>
#include "menus.h"
#include "menu.h"
#include "draw.h"
#include "menus/process_list.h"
#include "menus/process_patches.h"
#include "menus/n3ds.h"
#include "menus/debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"

#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tiny_jpeg.h"

Menu rosalinaMenu = {
    "Rosalina menu",
    .nbItems = 9,
    {
        { "Process list", METHOD, .method = &RosalinaMenu_ProcessList },
        { "Screenshot (L = Top, R = Bot)", METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { "New 3DS menu...", MENU, .menu = &N3DSMenu },
        { "Debugger options...", MENU, .menu = &debuggerMenu },
        { "System configuration...", MENU, .menu = &sysconfigMenu },
        { "Miscellaneous options...", MENU, .menu = &miscellaneousMenu },
        { "Power off", METHOD, .method = &RosalinaMenu_PowerOff },
        { "Reboot", METHOD, .method = &RosalinaMenu_Reboot },
        { "Credits", METHOD, .method = &RosalinaMenu_ShowCredits }
    }
};

void RosalinaMenu_ShowCredits(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Luma3DS credits");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2017 AuroraWright, TuxSH") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "3DSX loading code by fincs");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Networking code & basic GDB functionality by Stary");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "InputRedirection by Stary (PoC by ShinyQuagsire)");

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            (
                "Special thanks to:\n"
                "  Bond697, WinterMute, yifanlu,\n"
                "  Luma3DS contributors, ctrulib contributors,\n"
                "  other people"
            ));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);
}

void RosalinaMenu_Reboot(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to reboot, press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
            svcKernelSetState(7);
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

void RosalinaMenu_PowerOff(void) // Soft shutdown.
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina menu");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A to power off, press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & BUTTON_A)
        {
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
        }
        else if(pressed & BUTTON_B)
            return;
    }
    while(!terminationRequest);
}

static void tje_writer(void *buf, void *data, int size)
{
    memcpy(*(u8 **) buf, data, size);
    *(u8 **) buf += size;
}

enum screens
{
    top,
    bottom,
    both
};

void RosalinaMenu_TakeScreenshot(void)
{
#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

    static u8 rgb_buffer[3 * 400 * 480];
    static u8 jpeg_buffer[512 * 1024];
    u8 *jpeg_cursor = jpeg_buffer;

    u64 total;
    IFile file;
    Result res;

    u32 buttons = HID_PAD;
    int width = 0, height = 0;

    enum screens screen = both;
    if (buttons & BUTTON_L1)
        screen = top;
    else if (buttons & BUTTON_R1)
        screen = bottom;


    char filename[64];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    if(R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203))) svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();

    svcFlushEntireDataCache();

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if(R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if((u32)res == 0xC82044BE) // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

    u16 seconds, minutes, hours, days, year, month;
    u64 milliseconds = osGetTime();
    seconds = milliseconds/1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;

    year = 1900; // osGetTime starts in 1900

    while(1)
{
    bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    uint16_t daysInYear = leapYear ? 366 : 365;
    if (days >= daysInYear)
    {
        days -= daysInYear;
        ++year;
    }
    else
    {
        static const uint8_t daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        for(month = 0; month < 12; ++month)
        {
            uint8_t dim = daysInMonth[month];

            if (month == 1 && leapYear)
                ++dim;

            if (days >= dim)
                days -= dim;
            else
                break;
        }
        break;
    }
  }
    days++;
    month++;

sprintf(filename, "/luma/screenshots/%04u-%02u-%02u_%02u-%02u.%02u.jpg", year, month, days, hours, minutes, seconds);
TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));


    switch (screen)
    {
    case both:
        // Bottom screen
        for(u32 y = 0; y < 240; y++)
            Draw_ConvertFrameBufferLine(rgb_buffer + 3 * 400 * (y + 240), false, true, 239 - y);
        height += 240;

    // Falls through
    case top:
        // Top screen
        for(u32 y = 0; y < 240; y++)
            Draw_ConvertFrameBufferLine(rgb_buffer + 3 * 400 * y, true, true, 239 - y);
        width = 400;
        height += 240;
        break;

    case bottom:
        for(u32 y = 0; y < 240; y++)
            Draw_ConvertFrameBufferLine(rgb_buffer + 3 * 320 * y, false, false, 239 - y);
        width = 320;
        height = 240;
    }

    tje_encode_with_func(tje_writer, &jpeg_cursor, 3, width, height, 3, rgb_buffer);
    TRY(IFile_Write(&file, &total, jpeg_buffer, jpeg_cursor - jpeg_buffer, 0));
    TRY(IFile_Close(&file));

end:
    IFile_Close(&file);
    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screenshot");
        if(R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08x).", (u32)res);
        else
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while(!(waitInput() & BUTTON_B) && !terminationRequest);

#undef TRY
}
