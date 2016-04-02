/*
*   draw.c
*       by Reisyukaku / Aurora Wright
*   Code to print to the screen by mid-kid @CakesFW
*
*   Copyright (c) 2016 All Rights Reserved
*/

#include "draw.h"
#include "fs.h"
#include "memory.h"
#include "font.h"

#define SCREEN_TOP_WIDTH 400
#define SCREEN_TOP_HEIGHT 240

static const struct fb {
    u8 *top_left;
    u8 *top_right;
    u8 *bottom;
} *const fb = (struct fb *)0x23FFFE00;

static inline int strlen(const char *string)
{
    char *stringEnd = (char *)string;
    while(*stringEnd) stringEnd++;
    return stringEnd - string;
}

void clearScreens(void)
{
    memset32(fb->top_left, 0, 0x46500);
    memset32(fb->top_right, 0, 0x46500);
    memset32(fb->bottom, 0, 0x38400);
}

void loadSplash(void)
{
    clearScreens();

    //Don't delay boot if no splash image is on the SD
    if(fileRead(fb->top_left, "/aurei/splash.bin", 0x46500) +
       fileRead(fb->bottom, "/aurei/splashbottom.bin", 0x38400))
    {
        u64 i = 0x1300000;
        while(--i) __asm("mov r0, r0"); //Less Ghetto sleep func
    }
}

void drawCharacter(char character, int posX, int posY, u32 color)
{
    u8 *const select = fb->top_left;

    for(int y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for(int x = 7; x >= 0; x--)
        {
            int screenPos = (posX * SCREEN_TOP_HEIGHT * 3 + (SCREEN_TOP_HEIGHT - y - posY - 1) * 3) + (7 - x) * 3 * SCREEN_TOP_HEIGHT;

            if ((charPos >> x) & 1)
            {
                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
        }
    }
}

int drawString(const char *string, int posX, int posY, u32 color)
{
    int length = strlen(string);

    for(int i = 0, line_i = 0; i < length; i++, line_i++)
    {
        if(string[i] == '\n')
        {
            posY += SPACING_Y;
            line_i = 0;
            i++;
        } else if(line_i >= (SCREEN_TOP_WIDTH - posX) / SPACING_X)
        {
            // Make sure we never get out of the screen.
            posY += SPACING_Y;
            line_i = 2;  // Little offset so we know the same string continues.
            if(string[i] == ' ') i++;  // Spaces at the start look weird
        }

        drawCharacter(string[i], posX + line_i * SPACING_X, posY, color);
    }

    return posY;
}