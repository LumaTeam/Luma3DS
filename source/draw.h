/*
*   draw.h
*       by Reisyukaku / Aurora Wright
*   Code to print to the screen by mid-kid @CakesFW
*
*   Copyright (c) 2016 All Rights Reserved
*/

#pragma once

#include "types.h"

#define SPACING_Y 10
#define SPACING_X 8

void loadSplash(void);
void clearScreens(void);
void drawCharacter(char character, int posX, int posY, u32 color);
int drawString(const char *string, int posX, int posY, u32 color);