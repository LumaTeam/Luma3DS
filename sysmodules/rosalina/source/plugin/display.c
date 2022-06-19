#include <3ds.h>
#include "menu.h"
#include "draw.h"
#include <stdio.h>

static const char *__press_b_to_close = "Press [B] to close";

void    DispMessage(const char *title, const char *message)
{
    menuEnter();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, title);

    Draw_DrawString(30, 30, COLOR_WHITE, message);
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);


    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!preTerminationRequested && !(keys & KEY_B));

    Draw_Unlock(); ///< Keep it locked until we exit the message
    menuLeave();
}

u32    DispErrMessage(const char *title, const char *message, const Result error)
{
    char buf[100];

    sprintf(buf, "Error code: 0x%08lX", error);
    menuEnter();

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    Draw_DrawString(10, 10, COLOR_TITLE, title);

    u32 posY = Draw_DrawString(30, 30, COLOR_WHITE, message);
    Draw_DrawString(30, posY + 20, COLOR_RED, buf);
    Draw_DrawString(200, 220, COLOR_TITLE, __press_b_to_close);

    u32 keys = 0;

    do
    {
        keys = waitComboWithTimeout(1000);
    }while (!preTerminationRequested && !(keys & KEY_B));

    Draw_Unlock();  ///< Keep it locked until we exit the message
    menuLeave();
    return error;
}

typedef char string[50];
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void    DisplayPluginMenu(u32   *cmdbuf)
{
    u32             cursor = 0;
    u32             nbItems = cmdbuf[1];
    u8              *states = (u8 *)cmdbuf[3];
    char            buffer[60];
    const char      *title = (const char *)cmdbuf[5];
    const string    *items = (const string *)cmdbuf[7];
    const string    *hints = (const string *)cmdbuf[9];

    menuEnter();
    Draw_Lock();

    do
    {
        // Draw the menu
        {
            // Clear screen
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();

            // Draw title
            Draw_DrawString(10, 10, COLOR_TITLE, title);

            // Draw items
            u32 i = MAX(0, (int)cursor - 7);
            u32 end = MIN(nbItems, i + 16);
            u32 posY = 30;

            for (; i < end; ++i, posY += 10)
            {
                sprintf(buffer, "[ ] %s", items[i]);
                Draw_DrawString(30, posY, COLOR_WHITE, buffer);

                if (i == cursor) Draw_DrawCharacter(10, posY, COLOR_TITLE, '>');
                if (states[i]) Draw_DrawCharacter(36, posY, COLOR_LIME, 'x');
            }

            // Draw hint
            if (hints[cursor][0])
                Draw_DrawString(10, 200, COLOR_TITLE, hints[cursor]);
        }

        // Wait for input
        u32 pressed = waitInput();

        if (pressed & KEY_A)
            states[cursor] = !states[cursor];

        if (pressed & KEY_B)
            break;

        if (pressed & KEY_DOWN)
            if (++cursor >= nbItems)
                cursor = 0;

        if (pressed & KEY_UP)
            if (--cursor >= nbItems)
                cursor = nbItems - 1;

    } while (true);

    Draw_Unlock();
    menuLeave();
}
