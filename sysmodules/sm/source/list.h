/*
list.h

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds/types.h>

void buildList(void *list, void *pool, u32 nb, u32 elementSize);
void moveNode(void *node, void *dst, bool back);
void *allocateNode(void *inUseList, void *freeList, u32 elementSize, bool back);
