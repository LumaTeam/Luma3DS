#pragma once

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

u32 soc_init (void);
u32 soc_exit (void);

static u32 *SOC_buffer = 0;
