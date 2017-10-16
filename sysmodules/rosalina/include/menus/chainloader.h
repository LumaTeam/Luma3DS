

#pragma once

#include <3ds/types.h>
#include "menu.h"


u32 copy_firm_to_buf(FS_Archive sdmcArchive, const char *Path);
void bootloader(void);