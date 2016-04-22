/*
*   utils.h
*/

#pragma once

#include "types.h"

u32 waitInput(void);
void mcuShutDown(void);
void mcuReboot(void);
void error(const char *message);