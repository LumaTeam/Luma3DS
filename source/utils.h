/*
*   utils.h
*/

#pragma once

#include "types.h"

u32 waitInput(void);
void mcuReboot(void);

#define TICKS_PER_SEC   67027964ULL

void chrono(u32 seconds);
void stopChrono(void);