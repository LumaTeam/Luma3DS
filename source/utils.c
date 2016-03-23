/*
*   utils.c
*       by Aurora Wright
*   Copyright (c) 2016 All Rights Reserved
*/

#include "utils.h"
#include "screeninit.h"
#include "draw.h"
#include "fs.h"
#include "i2c.h"
#include "buttons.h"

//Number of options that can be configured
#define OPTIONS 3

#define COLOR_TITLE    0xFF9900
#define COLOR_WHITE    0xFFFFFF
#define COLOR_RED      0x0000FF
#define COLOR_BLACK    0x000000

struct options {
    char *text[OPTIONS];
    int pos_y[OPTIONS];
    u32 enabled[OPTIONS];
    u32 selected;
};

static u16 waitInput(void){
    u32 pressedkey = 0;
    u16 key;

    //Wait for no keys to be pressed
    while(HID_PAD);

    do {
        //Wait for a key to be pressed
        while(!HID_PAD);
        key = HID_PAD;

        //Make sure it's pressed
        for(u32 i = 0x13000; i; i--){
            if (key != HID_PAD)
                break;
            if(i==1) pressedkey = 1;
        }
    } while (!pressedkey);

    return key;
}

void configureCFW(const char *configPath){
    struct options options;

    options.text[0] = "( ) Updated SysNAND mode (A9LH-only)";
    options.text[1] = "( ) Use pre-patched FIRMs";
    options.text[2] = "( ) Force A9LH detection";

    initScreens();

    drawString("AuReiNand configuration", 10, 10, COLOR_TITLE);
    drawString("Press A to select, START to save and reboot", 10, 30, COLOR_WHITE);

    //Read and parse the existing configuration
    u16 tempConfig = 0;
    fileRead((u8 *)&tempConfig, configPath, 2);
    for(u32 i = 0; i < OPTIONS; i++)
        options.enabled[i] = (tempConfig >> i) & 0x1;

    //Pre-select the first configuration option
    options.selected = 0;

    //Boring configuration menu
    while(1){
        u16 pressed = 0;

        do{
            for(u32 i = 0; i < OPTIONS; i++){
                options.pos_y[i] = drawString(options.text[i], 10, !i ? 60 : options.pos_y[i - 1] + SPACING_VERT, options.selected == i ? COLOR_RED : COLOR_WHITE);
                drawCharacter('x', 10 + SPACING_HORIZ, options.pos_y[i], options.enabled[i] ? (options.selected == i ? COLOR_RED : COLOR_WHITE) : COLOR_BLACK);
            }
            pressed = waitInput();
        } while(!(pressed & (BUTTON_UP | BUTTON_DOWN | BUTTON_A | BUTTON_START)));

        if(pressed == BUTTON_UP) options.selected = !options.selected ? OPTIONS - 1 : options.selected - 1;
        else if(pressed == BUTTON_DOWN) options.selected = options.selected == OPTIONS - 1 ? 0 : options.selected + 1;
        else if(pressed == BUTTON_A) options.enabled[options.selected] = !options.enabled[options.selected];
        else if(pressed == BUTTON_START) break;
    }

    //Preserve the last-used boot options (second byte)
    tempConfig &= 0xFF00;

    //Parse and write the selected options
    for(u32 i = 0; i < OPTIONS; i++)
        tempConfig |= options.enabled[i] << i;
    fileWrite((u8 *)&tempConfig, configPath, 2);

    //Reboot
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(1);
}

void deleteFirms(const char *firmPaths[], u32 firms){
    while(firms){
        fileDelete(firmPaths[firms - 1]);
        firms--;
    }
}

void error(const char *message){
    initScreens();

    drawString("An error has occurred:", 10, 10, COLOR_RED);
    int pos_y = drawString(message, 10, 30, COLOR_WHITE);
    drawString("Press any button to shutdown", 10, pos_y + 2 * SPACING_VERT, COLOR_WHITE);

    waitInput();

    //Shutdown
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1);
    while(1);
}