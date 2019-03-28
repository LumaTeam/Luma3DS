#pragma once

#include <3ds/exheader.h>

// Official PM uses an overly complicated allocator with semaphores

void ExHeaderInfoHeap_Init(void *buf, size_t num);
ExHeader_Info *ExHeaderInfoHeap_New(void);
void ExHeaderInfoHeap_Delete(ExHeader_Info *data);
