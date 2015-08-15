/*
*   emunand.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#ifndef EMU_INC
#define EMU_INC

#include "types.h"

#define NCSD_MAGIC (0x4453434E)

u8 getEmunand(u32 *off, u32 *head);

#endif