#pragma once

#include "exploitdata.h"

s32 load_arm9_payload (char *filename);
s32 load_arm9_payload_from_mem (u8* data, u32 dsize);
void redirect_codeflow (u32 *dst_addr, u32 *src_addr);
void do_gshax_copy (void *dst, void *src, u32 len);
void priv_write_four (u32 address);
void user_clear_icache (void);
s32 corrupt_svcCreateThread (void);
s32 map_arm9_payload (void);
s32 map_arm11_payload (void);
void exploit_arm9_race_condition (void);
void repair_svcCreateThread (void);
s32 get_exploit_data (struct exploit_data *data);
s32 firm_reboot ();

#define BRAHMA_NETWORK_PORT 80

#define ARM_JUMPOUT 0xE51FF004 // LDR PC, [PC, -#04]
#define ARM_RET     0xE12FFF1E // BX LR
#define ARM_NOP     0xE1A00000 // NOP

static u8  *g_ext_arm9_buf;
static u32 g_ext_arm9_size = 0;
static s32 g_ext_arm9_loaded = 0;

extern void *arm11_start;
extern void *arm11_end;
extern void *arm9_start;
extern void *arm9_end;
