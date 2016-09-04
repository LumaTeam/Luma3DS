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
*   pin.c
*   Code to manage pin locking for 3ds. By reworks.
*/

#include "draw.h"
#include "config.h"
#include "screen.h"
#include "utils.h"
#include "memory.h"
#include "buttons.h"
#include "fs.h"
#include "pin.h"
#include "crypto.h"

static char pinKeyToLetter(u32 pressed)
{
    const char keys[] = "AB--RLUD--XY";

    u32 i;
    for(i = 31; pressed > 1; i--) pressed /= 2;

    return keys[31 - i];
}

void newPin(bool allowSkipping)
{
    clearScreens();

    char *title = allowSkipping ? "Press START to skip or enter a new PIN" : "Enter a new PIN to proceed";
    drawString(title, 10, 10, COLOR_TITLE);
    drawString("PIN: ", 10, 10 + 2 * SPACING_Y, COLOR_WHITE);

    //Pad to AES block length with zeroes
    u8 __attribute__((aligned(4))) enteredPassword[0x10] = {0};

    u8 length = 4 + 2 * (CONFIG_PIN - 1),
       cnt = 0;
    int charDrawPos = 5 * SPACING_X;

    while(cnt < length)
    {
        u32 pressed;
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & PIN_BUTTONS));

        pressed &= PIN_BUTTONS;
        if(!allowSkipping) pressed &= ~BUTTON_START;

        if(pressed & BUTTON_START) return;
        if(!pressed) continue;

        char key = pinKeyToLetter(pressed);
        enteredPassword[cnt++] = (u8)key; //Add character to password

        //Visualize character on screen
        drawCharacter(key, 10 + charDrawPos, 10 + 2 * SPACING_Y, COLOR_WHITE);
        charDrawPos += 2 * SPACING_X;
    }

    PINData pin;

    memcpy(pin.magic, "PINF", 4);
    pin.formatVersionMajor = PIN_VERSIONMAJOR;
    pin.formatVersionMinor = PIN_VERSIONMINOR;
    pin.length = length;

    u8 __attribute__((aligned(4))) tmp[0x20];
    u8 __attribute__((aligned(4))) zeroes[0x10] = {0};

    computePinHash(tmp, zeroes);
    memcpy(pin.testHash, tmp, sizeof(tmp));

    computePinHash(tmp, enteredPassword);
    memcpy(pin.hash, tmp, sizeof(tmp));

    if(!fileWrite(&pin, PIN_PATH, sizeof(PINData)))
        error("Error writing the PIN file");
}

bool verifyPin(void)
{
    initScreens();

    PINData pin;

    if(fileRead(&pin, PIN_PATH) != sizeof(PINData) ||
       memcmp(pin.magic, "PINF", 4) != 0 ||
       pin.formatVersionMajor != PIN_VERSIONMAJOR ||
       pin.formatVersionMinor != PIN_VERSIONMINOR ||
       pin.length != 4 + 2 * (CONFIG_PIN - 1))
        return false;

    u8 __attribute__((aligned(4))) zeroes[0x10] = {0};
    u8 __attribute__((aligned(4))) tmp[0x20];

    computePinHash(tmp, zeroes);

    //Test vector verification (SD card has, or hasn't been used on another console)
    if(memcmp(pin.testHash, tmp, sizeof(tmp)) != 0) return false;

    //Pad to AES block length with zeroes
    u8 __attribute__((aligned(4))) enteredPassword[0x10] = {0};

    u8 cnt = 0;
    bool unlock = false;
    int charDrawPos = 5 * SPACING_X;

    while(!unlock)
    {
        drawString("Press START to shutdown or enter PIN to proceed", 10, 10, COLOR_TITLE);
        drawString("PIN: ", 10, 10 + 2 * SPACING_Y, COLOR_WHITE);

        u32 pressed;
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & PIN_BUTTONS));

        if(pressed & BUTTON_START) mcuPowerOff();

        pressed &= PIN_BUTTONS;

        if(!pressed) continue;

        char key = pinKeyToLetter(pressed);
        enteredPassword[cnt++] = (u8)key; //Add character to password

        //Visualize character on screen
        drawCharacter(key, 10 + charDrawPos, 10 + 2 * SPACING_Y, COLOR_WHITE);
        charDrawPos += 2 * SPACING_X;

        if(cnt >= pin.length)
        {
            computePinHash(tmp, enteredPassword);
            unlock = memcmp(pin.hash, tmp, sizeof(tmp)) == 0;

            if(!unlock)
            {
                charDrawPos = 5 * SPACING_X;
                cnt = 0;

                clearScreens();

                drawString("Wrong PIN, try again", 10, 10 + 4 * SPACING_Y, COLOR_RED); 
            }
        }
    }

    return true;
}