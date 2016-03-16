#pragma once

#include "fs.h"
#include "i2c.h"

// Linux settings
#define ZIMAGE_ADDR    (0x20008000)
#define PARAMS_ADDR    (0x20000100)
#define LINUXIMAGE_FILENAME "/zImage"
#define DTB_FILENAME        "/nintendo3ds_ctr.dtb"

void runLinux(void);
void shutdown(void);
void loadLinux(void);
void arm11(void);