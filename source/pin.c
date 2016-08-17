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
#include "screen.h"
#include "utils.h"
#include "memory.h"
#include "buttons.h"
#include "fs.h"
#include "pin.h"
#include "crypto.h"

bool readPin(PINData *out)
{
    u8 __attribute__((aligned(4))) zeroes[16] = {0};
    u8 __attribute__((aligned(4))) tmp[32] = {0};

    if(fileRead(out, "/luma/pin.bin") != sizeof(PINData)) return false;
    if(memcmp(out->magic, "PINF", 4) != 0) return false;

    computePINHash(tmp, zeroes, 1);

    return memcmp(out->testHash, tmp, 32) == 0; //test vector verification (SD card has (or hasn't) been used on another console)
}

static inline char PINKeyToLetter(u32 pressed)
{
    const char keys[] = "AB--------XY";

    u32 i;
    __asm__ volatile("clz %[i], %[pressed]" : [i] "=r" (i) : [pressed] "r" (pressed));

    return keys[31 - i];
}

void newPin(void)
{
    clearScreens();

    drawString("Enter your NEW PIN: ", 10, 10, COLOR_WHITE);

    // Set the default value as 0x00 so we can check if there are any unentered characters.
    u8 __attribute__((aligned(4))) enteredPassword[16 * ((PIN_LENGTH + 15) / 16)] = {0}; // pad to AES block length

    u32 cnt = 0;
    int charDrawPos = 20 * SPACING_X;

    while(cnt < PIN_LENGTH)
    {
        u32 pressed;
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & PIN_BUTTONS & ~BUTTON_START));

        pressed &= PIN_BUTTONS & ~BUTTON_START;

        if(!pressed) continue;
        char key = PINKeyToLetter(pressed);
        enteredPassword[cnt++] = (u8)key; // add character to password.

        // visualize character on screen.
        drawCharacter(key, 10 + charDrawPos, 10, COLOR_WHITE);
        charDrawPos += 2 * SPACING_X;
    }

    PINData pin = {0};
    u8 __attribute__((aligned(4))) tmp[32] = {0};
    u8 __attribute__((aligned(4))) zeroes[16] = {0};

    memcpy(pin.magic, "PINF", 4);
    pin.formatVersionMajor = 1;
    pin.formatVersionMinor = 0;

    computePINHash(tmp, zeroes, 1);
    memcpy(pin.testHash, tmp, 32);

    computePINHash(tmp, enteredPassword, (PIN_LENGTH + 15) / 16);
    memcpy(pin.hash, tmp, 32);

    fileWrite(&pin, "/luma/pin.bin", sizeof(PINData));
    
    while(HID_PAD & PIN_BUTTONS);
}

void verifyPin(PINData *in)
{
    initScreens();

    drawString("Press START to shutdown or enter pin to proceed.", 10, 10, COLOR_WHITE);
    drawString("Pin: ", 10, 10 + 2 * SPACING_Y, COLOR_WHITE);

    // Set the default characters as 0x00 so we can check if there are any unentered characters.
    u8 __attribute__((aligned(4))) enteredPassword[16 * ((PIN_LENGTH + 15) / 16)] = {0};

    u32 cnt = 0;
    bool unlock = false;
    int charDrawPos = 5 * SPACING_X;

    while(!unlock)
    {
        u32 pressed;
        do
        {
            pressed = waitInput();
        }
        while(!(pressed & PIN_BUTTONS));

        if(pressed & BUTTON_START) mcuPowerOff();

        pressed &= PIN_BUTTONS & ~BUTTON_START;
        if(!pressed) continue;

        char key = PINKeyToLetter(pressed);
        enteredPassword[cnt++] = (u8)key; // add character to password.

        // visualize character on screen.
        drawCharacter(key, 10 + charDrawPos, 10 + 2 * SPACING_Y, COLOR_WHITE);
        charDrawPos += 2 * SPACING_X;

        if(cnt >= PIN_LENGTH)
        {
            u8 __attribute__((aligned(4))) tmp[32] = {0};

            computePINHash(tmp, enteredPassword, (PIN_LENGTH + 15) / 16);
            unlock = memcmp(in->hash, tmp, 32) == 0;

            if(!unlock)
            {
                charDrawPos = 5 * SPACING_X;
                cnt = 0;

                clearScreens();

                drawString("Press START to shutdown or enter pin to proceed.", 10, 10, COLOR_WHITE);
                drawString("Pin: ", 10, 10 + 2 * SPACING_Y, COLOR_WHITE);
                drawString("Wrong pin! Try again!", 10, 10 + 3 * SPACING_Y, COLOR_RED); 
            }
        }
    }
}