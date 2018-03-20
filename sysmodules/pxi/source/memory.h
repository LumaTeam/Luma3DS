/*
memory.h:
    Memory functions

(c) TuxSH, 2016-2017
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds/types.h>

void memcpy(void *dest, const void *src, u32 size);
