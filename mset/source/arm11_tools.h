#ifndef __arm11_tools_h__
#define __arm11_tools_h__

#include <stdint.h>

void invalidate_data_cache();
void invalidate_instruction_cache();
void asm_memcpy(void *dest, void *src, uint32_t length);

#endif
