#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include "brahma.h"
#include "hid.h"

#ifndef LAUNCHER_PATH
#define LAUNCHER_PATH "Cakes.dat"
#endif

int main (void) {
    if (brahma_init()) {
        if (load_arm9_payload_offset("/" LAUNCHER_PATH, 0x12000, 0x10000) != 1)
            goto error;
        firm_reboot();
        brahma_exit();
    }

    // Return to hbmenu
    return 0;

error:
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    printf("An error occurred while loading the payload.\nMake sure your launcher is located at:\n/" LAUNCHER_PATH);
    wait_any_key();

    gfxExit();
    return 1;
}
