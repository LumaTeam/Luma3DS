#include <3ds.h>
#include <stdio.h>
#include "brahma.h"
#include "hid.h"

s32 main (void) {
	// Initialize services
	srvInit();
	aptInit();
	hidInit(NULL);
	gfxInitDefault();
	fsInit();
	sdmcInit();
	hbInit();
	qtmInit();
    
    gfxSwapBuffers();

    u32 payload_size = 0x10000;
    void *payload = malloc(payload_size);

    FILE *fp = fopen("/reiNand.dat", "r");
    if (!fp) goto exit;
    fseek(fp, 0x12000, SEEK_SET);
    fread(payload, payload_size, 1, fp);
    fclose(fp);

    if (brahma_init()) {
        load_arm9_payload_from_mem(payload, payload_size);
        firm_reboot();
        brahma_exit();
    }

exit:
    if (payload) free(payload);

	hbExit();
	sdmcExit();
	fsExit();
	gfxExit();
	hidExit();
	aptExit();
	srvExit();
    return 0;
}
