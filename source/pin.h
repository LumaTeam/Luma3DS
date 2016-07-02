/*
*   pin.h
*
*   Code to manage pin locking for 3ds. By reworks.
*/

#pragma once

#include "types.h"

bool readPin(uint8_t* out);
bool writePin(uint8_t* in);
void deletePin(void);

//bool validateFile(void);
bool doesPinExist(void);

void newPin(void);
void verifyPin(bool allowQuit);