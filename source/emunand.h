/*
*   emunand.h
*/

#pragma once

#include "types.h"

#define NCSD_MAGIC 0x4453434E

void locateEmuNAND(u32 *off, u32 *head, u32 *emuNAND);
void patchEmuNAND(u8 *arm9Section, u32 arm9SectionSize, u8 *process9Offset, u32 process9Size, u32 emuOffset, u32 emuHeader, u32 branchAdditive);