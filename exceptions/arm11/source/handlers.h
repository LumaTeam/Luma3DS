/*
*   handlers.h
*       by TuxSH
*
*   This is part of Luma3DS, see LICENSE.txt for details
*/

#pragma once

void __attribute__((noreturn)) mcuReboot(void);
void clearDCacheAndDMB(void);

void FIQHandler(void);
void undefinedInstructionHandler(void);
void dataAbortHandler(void);
void prefetchAbortHandler(void);