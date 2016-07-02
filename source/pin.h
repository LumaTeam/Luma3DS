/*
*   pin.h
*
*   Code to manage pin locking for 3ds. By reworks.
*/

#pragma once

#include "types.h"

void readPin(uint8_t* out);
void writePin(uint8_t* in);
void deletePin(void);

//bool validateFile(void);
bool doesPinExist(void);

void newPin(void);
void verifyPin(bool allowQuit);