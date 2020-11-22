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

/*
*   Code to print to the screen by mid-kid @CakesFW
*      https://github.com/mid-kid/CakesForeveryWan/
*/

#pragma once

#include "types.h"

#define SPACING_Y 10
#define SPACING_X 8

#define COLOR_TITLE  0xFF9900
#define COLOR_WHITE  0xFFFFFF
#define COLOR_RED    0x0000FF
#define COLOR_BLACK  0x000000
#define COLOR_YELLOW 0x00FFFF

#define DRAW_MAX_FORMATTED_STRING_SIZE  512

bool loadSplash(void);
void drawCharacter(bool isTopScreen, u32 posX, u32 posY, u32 color, char character);
u32 drawString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *string);

__attribute__((format(printf,5,6)))
u32 drawFormattedString(bool isTopScreen, u32 posX, u32 posY, u32 color, const char *fmt, ...);
