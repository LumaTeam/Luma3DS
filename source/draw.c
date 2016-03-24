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

static int strlen(const char *string){
    char *stringEnd = (char *)string;
    while(*stringEnd) stringEnd++;
    return stringEnd - string;
}

void clearScreens(void){
    memset32(fb->top_left, 0, 0x46500);
    memset32(fb->top_right, 0, 0x46500);
    memset32(fb->bottom, 0, 0x38400);
}

void loadSplash(void){
    clearScreens();

    //Don't delay boot if no splash image is on the SD
    if(fileRead(fb->top_left, "/aurei/splash.bin", 0x46500) +
       fileRead(fb->bottom, "/aurei/splashbottom.bin", 0x38400)){
        u64 i = 0x1300000; while(--i) __asm("mov r0, r0"); //Less Ghetto sleep func
    }
}

void drawCharacter(char character, int pos_x, int pos_y, u32 color){
    u8 *const select = fb->top_left;

    for(int y = 0; y < 8; y++){
        unsigned char char_pos = font[character * 8 + y];

        for(int x = 7; x >= 0; x--){
            int screen_pos = (pos_x * SCREEN_TOP_HEIGHT * 3 + (SCREEN_TOP_HEIGHT - y - pos_y - 1) * 3) + (7 - x) * 3 * SCREEN_TOP_HEIGHT;

            if ((char_pos >> x) & 1) {
                select[screen_pos] = color >> 16;
                select[screen_pos + 1] = color >> 8;
                select[screen_pos + 2] = color;
            }
        }
    }
}

int drawString(const char *string, int pos_x, int pos_y, u32 color){
    int length = strlen(string);

    for(int i = 0, line_i = 0; i < length; i++, line_i++){
        if(string[i] == '\n'){
            pos_y += SPACING_VERT;
            line_i = 0;
            i++;
        } else if(line_i >= (SCREEN_TOP_WIDTH - pos_x) / SPACING_HORIZ){
            // Make sure we never get out of the screen.
            pos_y += SPACING_VERT;
            line_i = 2;  // Little offset so we know the same string continues.
            if(string[i] == ' ') i++;  // Spaces at the start look weird
        }

        drawCharacter(string[i], pos_x + line_i * SPACING_HORIZ, pos_y, color);
    }

    return pos_y;
}