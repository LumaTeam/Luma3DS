/*
*   pin.h
*
*   Code to manage pin locking for 3ds. By reworks.
*/

#pragma once

#include "types.h"

// Stands for Pin Container
typedef struct PinCont
{
	char pin[4];
} PinCont;

PinCont readPin(void);
void writePin(PinCont* pin);
bool doesPinExist(void);
void deletePin(void);

void newPin(void);
void verifyPin(void);