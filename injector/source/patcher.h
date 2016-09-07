#pragma once

#include <3ds/types.h>

#define PATH_MAX 255

#define CONFIG(a)        (((info.config >> (a + 21)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 7)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)

#define BOOTCFG_NAND               BOOTCONFIG(0, 7)
#define BOOTCFG_FIRM               BOOTCONFIG(3, 1)
#define BOOTCFG_SAFEMODE           BOOTCONFIG(6, 1)
#define CONFIG_NEWCPU              MULTICONFIG(3)
#define CONFIG_DEVOPTIONS          MULTICONFIG(4)
#define CONFIG_USESYSFIRM          CONFIG(1)
#define CONFIG_USELANGEMUANDCODE   CONFIG(2)
#define CONFIG_SHOWNAND            CONFIG(3)

void patchCode(u64 progId, u8 *code, u32 size);