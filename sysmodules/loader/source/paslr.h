#pragma once

#include <3ds/types.h>
#include <3ds/exheader.h>

Result allocateProgramMemory(const ExHeader_Info *exhi, u32 vaddr, u32 size);
