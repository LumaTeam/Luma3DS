#pragma once

#include <3ds/types.h>

float Volume_ExtractVolume(int nul, int one, int slider);
void Volume_AdjustVolume(u8* out, int slider, float value);
void Volume_ControlVolume(void);
