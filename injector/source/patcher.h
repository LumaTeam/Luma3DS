#pragma once

#include <3ds/types.h>

#define PATH_MAX 255

#define CONFIG(a)        (((info.config >> (a + 20)) & 1) != 0)
#define MULTICONFIG(a)   ((info.config >> (a * 2 + 6)) & 3)
#define BOOTCONFIG(a, b) ((info.config >> a) & b)

#define BOOTCFG_NAND               BOOTCONFIG(0, 3)
#define BOOTCFG_FIRM               BOOTCONFIG(2, 1)
#define BOOTCFG_SAFEMODE           BOOTCONFIG(5, 1)
#define CONFIG_NEWCPU              MULTICONFIG(2)
#define CONFIG_USESYSFIRM          CONFIG(1)
#define CONFIG_USELANGEMUANDCODE   CONFIG(3)
#define CONFIG_SHOWNAND            CONFIG(4)

void patchCode(u64 progId, u8 *code, u32 size);