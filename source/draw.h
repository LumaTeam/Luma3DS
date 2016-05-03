/*
*   draw.h
*
*   Code to print to the screen by mid-kid @CakesFW
*/

#pragma once

#include "types.h"

#define SCREEN_TOP_WIDTH  400
#define SCREEN_TOP_HEIGHT 240

#define SPACING_Y 10
#define SPACING_X 8

#define COLOR_TITLE 0xFF9900
#define COLOR_WHITE 0xFFFFFF
#define COLOR_RED   0x0000FF
#define COLOR_BLACK 0x000000

void loadSplash(void);
void clearScreens(void);
void drawCharacter(char character, int posX, int posY, u32 color);
int drawString(const char *string, int posX, int posY, u32 color);