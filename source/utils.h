/*
*   utils.h
*/

#pragma once

#include "types.h"

u32 waitInput(void);
void mcuReboot(void);

#define TICKS_PER_SEC       67027964ULL
#define REG_TIMER_CNT(i)    *(vu16 *)(0x10003002 + 4 * i)
#define REG_TIMER_VAL(i)    *(vu16 *)(0x10003000 + 4 * i)

void chrono(u32 seconds);
void stopChrono(void);