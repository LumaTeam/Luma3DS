#pragma once

#include "exploitdata.h"

u32 brahma_init (void);
u32 brahma_exit (void);
s32 load_arm9_payload_offset (char *filename, u32 offset, u32 max_psize);
s32 load_arm9_payload_from_mem (u8* data, u32 dsize);
void redirect_codeflow (u32 *dst_addr, u32 *src_addr);
s32 map_arm9_payload (void);
s32 map_arm11_payload (void);
void exploit_arm9_race_condition (void);
s32 get_exploit_data (struct exploit_data *data);
s32 firm_reboot ();

#define load_arm9_payload(filename) load_arm9_payload_offset(filename, 0, 0)

#define BRAHMA_NETWORK_PORT 80

#define ARM_JUMPOUT 0xE51FF004 // LDR PC, [PC, -#04]
#define ARM_RET     0xE12FFF1E // BX LR
#define ARM_NOP     0xE1A00000 // NOP

extern void *arm11_start;
extern void *arm11_end;
extern void *arm9_start;
extern void *arm9_end;
