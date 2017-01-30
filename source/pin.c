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
*   Code originally by reworks
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

void newPin(bool allowSkipping, u32 pinMode)
{
    clearScreens(false);

    u8 length = 4 + 2 * (pinMode - 1);

    drawString("Enter a new PIN using ABXY and the DPad", true, 10, 10, COLOR_TITLE);
    drawString(allowSkipping ? "Press START to skip, SELECT to reset" : "Press SELECT to reset", true, 10, 10 + SPACING_Y, COLOR_TITLE);

    drawString("PIN (  digits): ", true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE);
    drawCharacter('0' + length, true, 10 + 5 * SPACING_X, 10 + 3 * SPACING_Y, COLOR_WHITE);

    //Pad to AES block length with zeroes
    __attribute__((aligned(4))) u8 enteredPassword[AES_BLOCK_SIZE] = {0};

    bool reset = false;
    u8 cnt = 0;

    while(cnt < length)
    {
        if(reset)
        {
            for(u32 i = 0; i < cnt; i++) 
                drawCharacter((char)enteredPassword[i], true, 10 + (16 + 2 * i) * SPACING_X, 10 + 3 * SPACING_Y, COLOR_BLACK);

            cnt = 0;
            reset = false;
        }

        u32 pressed;
        do
        {
            pressed = waitInput(false);
        }
        while(!(pressed & PIN_BUTTONS));

        pressed &= PIN_BUTTONS;
        if(!allowSkipping) pressed &= ~BUTTON_START;

        if(pressed & BUTTON_START) return;

        if(pressed & BUTTON_SELECT)
        {
            reset = true;
            continue;
        }

        if(!pressed) continue;

        //Add character to password
        enteredPassword[cnt] = (u8)pinKeyToLetter(pressed);

        //Visualize character on screen
        drawCharacter(enteredPassword[cnt], true, 10 + (16 + 2 * cnt) * SPACING_X, 10 + 3 * SPACING_Y, COLOR_WHITE);

        cnt++;
    }

    PinData pin;

    memcpy(pin.magic, "PINF", 4);
    pin.formatVersionMajor = PIN_VERSIONMAJOR;
    pin.formatVersionMinor = PIN_VERSIONMINOR;

    __attribute__((aligned(4))) u8 tmp[SHA_256_HASH_SIZE],
                                   lengthBlock[AES_BLOCK_SIZE] = {0};
    lengthBlock[0] = length;

    computePinHash(tmp, lengthBlock);
    memcpy(pin.lengthHash, tmp, sizeof(tmp));

    computePinHash(tmp, enteredPassword);
    memcpy(pin.hash, tmp, sizeof(tmp));

    if(!fileWrite(&pin, PIN_FILE, sizeof(PinData)))
        error("Error writing the PIN file");
}

bool verifyPin(u32 pinMode, bool hidePin)
{
    PinData pin;

    if(fileRead(&pin, PIN_FILE, sizeof(PinData)) != sizeof(PinData) ||
       memcmp(pin.magic, "PINF", 4) != 0 ||
       pin.formatVersionMajor != PIN_VERSIONMAJOR ||
       pin.formatVersionMinor != PIN_VERSIONMINOR)
        return false;

    __attribute__((aligned(4))) u8 tmp[SHA_256_HASH_SIZE],
                                   lengthBlock[AES_BLOCK_SIZE] = {0};
    lengthBlock[0] = 4 + 2 * (pinMode - 1);

    computePinHash(tmp, lengthBlock);

    //Test vector verification (check if SD card has been used on another console or PIN length changed)
    if(memcmp(pin.lengthHash, tmp, sizeof(tmp)) != 0) return false;

    initScreens();

    drawString("Enter the PIN using ABXY and the DPad to proceed", true, 10, 10, COLOR_TITLE);
    drawString("Press START to shutdown, SELECT to clear", true, 10, 10 + SPACING_Y, COLOR_TITLE);

    drawString("PIN (  digits): ", true, 10, 10 + 3 * SPACING_Y, COLOR_WHITE);
    drawCharacter('0' + lengthBlock[0], true, 10 + 5 * SPACING_X, 10 + 3 * SPACING_Y, COLOR_WHITE);

    const char *messageFile = "pinmessage.txt";
    char message[801];

    u32 messageSize = fileRead(message, messageFile, sizeof(message) - 1);

    if(messageSize != 0)
    {
        message[messageSize] = 0;
        drawString(message, false, 10, 10, COLOR_WHITE);
    }

    //Pad to AES block length with zeroes
    __attribute__((aligned(4))) u8 enteredPassword[AES_BLOCK_SIZE] = {0};

    bool unlock = false,
         reset = false;
    u8 cnt = 0;

    while(!unlock)
    {
        if(reset)
        {
            for(u32 i = 0; i < cnt; i++) 
                drawCharacter((char)enteredPassword[i], true, 10 + (16 + 2 * i) * SPACING_X, 10 + 3 * SPACING_Y, COLOR_BLACK);

            cnt = 0;
            reset = false;
        }

        u32 pressed;
        do
        {
            pressed = waitInput(false);
        }
        while(!(pressed & PIN_BUTTONS));

        if(pressed & BUTTON_START) mcuPowerOff();

        pressed &= PIN_BUTTONS;

        if(pressed & BUTTON_SELECT)
        {
            reset = true;
            continue;
        }

        if(!pressed) continue;

        //Add character to password
        enteredPassword[cnt] = (u8)pinKeyToLetter(pressed);
        //Visualize character on screen
        char display = hidePin ? '*' : (char)enteredPassword[cnt];
        drawCharacter(display, true, 10 + (16 + 2 * cnt) * SPACING_X, 10 + 3 * SPACING_Y, COLOR_WHITE);

        if(++cnt < lengthBlock[0]) continue;

        computePinHash(tmp, enteredPassword);
        unlock = memcmp(pin.hash, tmp, sizeof(tmp)) == 0;

        if(!unlock)
        {
            reset = true;

            drawString("Wrong PIN, try again", true, 10, 10 + 5 * SPACING_Y, COLOR_RED); 
        }
    }

    return true;
}