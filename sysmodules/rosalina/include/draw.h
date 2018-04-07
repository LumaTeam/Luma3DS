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

#pragma once

#include <3ds/types.h>
#include <3ds/gfx.h>
#include "utils.h"

#define GPU_FB_TOP_LEFT_ADDR_1      REG32(0x10400468)
#define GPU_FB_TOP_LEFT_ADDR_2      REG32(0x1040046C)
#define GPU_FB_TOP_FMT              REG32(0x10400470)
#define GPU_FB_TOP_SEL              REG32(0x10400478)
#define GPU_FB_TOP_STRIDE           REG32(0x10400490)
#define GPU_FB_TOP_RIGHT_ADDR_1     REG32(0x10400494)
#define GPU_FB_TOP_RIGHT_ADDR_2     REG32(0x10400498)

#define GPU_FB_BOTTOM_ADDR_1        REG32(0x10400568)
#define GPU_FB_BOTTOM_ADDR_2        REG32(0x1040056C)
#define GPU_FB_BOTTOM_FMT           REG32(0x10400570)
#define GPU_FB_BOTTOM_SEL           REG32(0x10400578)
#define GPU_FB_BOTTOM_STRIDE        REG32(0x10400590)

#define GPU_PSC0_CNT                REG32(0x1040001C)
#define GPU_PSC1_CNT                REG32(0x1040001C)

#define GPU_TRANSFER_CNT            REG32(0x10400C18)
#define GPU_CMDLIST_CNT             REG32(0x104018F0)

#define FB_BOTTOM_VRAM_ADDR         ((void *)0x1F48F000) // cached
#define FB_BOTTOM_VRAM_PA           0x1848F000
#define FB_BOTTOM_SIZE              (320 * 240 * 2)

#define SCREEN_BOT_WIDTH  320
#define SCREEN_BOT_HEIGHT 240

#define SPACING_Y 11
#define SPACING_X 6

#define COLOR_TITLE RGB565(0x00, 0x26, 0x1F)
#define COLOR_WHITE RGB565(0x1F, 0x3F, 0x1F)
#define COLOR_RED   RGB565(0x1F, 0x00, 0x00)
#define COLOR_GREEN RGB565(0x00, 0x1F, 0x00)
#define COLOR_BLACK RGB565(0x00, 0x00, 0x00)

#define DRAW_MAX_FORMATTED_STRING_SIZE  512

void Draw_Lock(void);
void Draw_Unlock(void);

void Draw_DrawCharacter(u32 posX, u32 posY, u32 color, char character);
u32 Draw_DrawString(u32 posX, u32 posY, u32 color, const char *string);
u32 Draw_DrawFormattedString(u32 posX, u32 posY, u32 color, const char *fmt, ...);

void Draw_FillFramebuffer(u32 value);
void Draw_ClearFramebuffer(void);
void Draw_SetupFramebuffer(void);
void Draw_RestoreFramebuffer(void);
void Draw_FlushFramebuffer(void);
u32 Draw_GetCurrentFramebufferAddress(bool top, bool left);

void Draw_CreateBitmapHeader(u8 *dst, u32 width, u32 heigth);
void Draw_ConvertFrameBufferLine(u8 *line, bool top, bool left, u32 y);
