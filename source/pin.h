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
*   pin.h
*
*   Code to manage pin locking for 3ds. By reworks.
*/

#pragma once

#include "types.h"

#define PIN_LENGTH  4

typedef struct __attribute__((packed))
{
    char magic[4];
    u16 formatVersionMajor, formatVersionMinor;

    u8 testHash[32];
    u8 hash[32];
} PINData;

bool readPin(PINData* out);
void newPin(void);
void verifyPin(PINData *in);