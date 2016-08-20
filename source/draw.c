/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
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
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

/*
*   Code to print to the screen by mid-kid @CakesFW
*      https://github.com/mid-kid/CakesForeveryWan/
*/

#include "draw.h"
#include "screen.h"
#include "utils.h"
#include "fs.h"
#include "font.h"

static inline int strlen(const char *string)
{
    char *stringEnd = (char *)string;

    while(*stringEnd) stringEnd++;

    return stringEnd - string;
}

bool loadSplash(void)
{
    //Don't delay boot nor init the screens if no splash image is on the SD
    if(getFileSize("/luma/splash.bin") + getFileSize("/luma/splashbottom.bin") == 0)
        return false;
    
    initScreens();

    fileRead(fb->top_left, "/luma/splash.bin");
    fileRead(fb->bottom, "/luma/splashbottom.bin");

    chrono(3);

    return true;
}

void drawCharacter(char character, int posX, int posY, u32 color)
{
    u8 *const select = fb->top_left;

    for(int y = 0; y < 8; y++)
    {
        char charPos = font[character * 8 + y];

        for(int x = 7; x >= 0; x--)
            if ((charPos >> x) & 1)
            {
                int screenPos = (posX * SCREEN_TOP_HEIGHT * 3 + (SCREEN_TOP_HEIGHT - y - posY - 1) * 3) + (7 - x) * 3 * SCREEN_TOP_HEIGHT;

                select[screenPos] = color >> 16;
                select[screenPos + 1] = color >> 8;
                select[screenPos + 2] = color;
            }
    }
}

int drawString(const char *string, int posX, int posY, u32 color)
{
    for(int i = 0, line_i = 0; i < strlen(string); i++, line_i++)
    {
        if(string[i] == '\n')
        {
            posY += SPACING_Y;
            line_i = 0;
            i++;
        }
        else if(line_i >= (SCREEN_TOP_WIDTH - posX) / SPACING_X)
        {
            // Make sure we never get out of the screen.
            posY += SPACING_Y;
            line_i = 2; //Little offset so we know the same string continues.
            if(string[i] == ' ') i++; //Spaces at the start look weird
        }

        drawCharacter(string[i], posX + line_i * SPACING_X, posY, color);
    }

    return posY;
}
