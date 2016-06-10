/*
*   utils.c
*/

#include "utils.h"
#include "i2c.h"
#include "buttons.h"
#include "memory.h"

u32 waitInput(void)
{
    u32 pressedKey = 0,
        key;

    //Wait for no keys to be pressed
    while(HID_PAD);

    do
    {
        //Wait for a key to be pressed
        while(!HID_PAD);

        key = HID_PAD;

        //Make sure it's pressed
        for(u32 i = 0x13000; i; i--)
        {
            if(key != HID_PAD) break;
            if(i == 1) pressedKey = 1;
        }
    }
    while(!pressedKey);

    return key;
}

void mcuReboot(void)
{
    cleanInvalidateDCacheAndDMB(); //Ensure that all memory transfers have completed and that the data cache has been flushed
    
    i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 2);
    while(1);
}

//TODO: add support for TIMER IRQ
static inline void startChrono(u64 initialTicks)
{
    //Based on a NATIVE_FIRM disassembly

    REG_TIMER_CNT(0) = 0; //67MHz
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 4; //Count-up

    for(u32 i = 0; i < 4; i++) REG_TIMER_VAL(i) = (u16)(initialTicks >> (16 * i));

    REG_TIMER_CNT(0) = 0x80; //67MHz; enabled
    for(u32 i = 1; i < 4; i++) REG_TIMER_CNT(i) = 0x84; //Count-up; enabled
}

void chrono(u32 seconds)
{
    static u64 startingTicks = 0;

    if(!startingTicks) startChrono(0);

    u64 res;

    do
    {
        res = 0;
        for(u32 i = 0; i < 4; i++) res |= REG_TIMER_VAL(i) << (16 * i);
    }
    while(res - startingTicks < seconds * TICKS_PER_SEC);

    if(!seconds) startingTicks = res;
}

void stopChrono(void)
{
    for(u32 i = 0; i < 4; i++) REG_TIMER_CNT(i) &= ~0x80;
}