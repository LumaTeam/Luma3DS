/*
*   handlers.h
*       by TuxSH
*   Copyright (c) 2016 All Rights Reserved
*/

// This is part of AuReiNand, see LICENSE.txt for details

#pragma once

void setupStack(u32 mode, void* stack);

void FIQHandler(void);
void undefinedInstructionHandler(void);
void dataAbortHandler(void);
void prefetchAbortHandler(void);