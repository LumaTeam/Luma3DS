#include "types.h"
#include "buttons.h"
#include "fatfs/ff.h"

#define PAYLOAD_ADDRESS	0x23F00000
#define LOAD_PAYLOAD(a) loadPayload(a "_*.bin")

static u32 loadPayload(const char *pattern)
{
    char path[30] = "/luma/payloads";

    DIR dir;
    FILINFO info;

    FRESULT result = f_findfirst(&dir, &info, path, pattern);

    f_closedir(&dir);

    if(result != FR_OK || !info.fname[0])
        return 0;

    path[14] = '/';
    u32 i;
    for(i = 0; info.fname[i]; i++)
        path[15 + i] = info.fname[i];
    path[15 + i] = '\0';

    FIL payload;
    unsigned int br;

    f_open(&payload, path, FA_READ);
    f_read(&payload, (void *)PAYLOAD_ADDRESS, f_size(&payload), &br);
    f_close(&payload);

    return 1;
}

void main(void)
{
    FATFS fs;

    f_mount(&fs, "0:", 1);

    //Get pressed buttons
    u32 pressed = HID_PAD;

    if(((pressed & BUTTON_RIGHT) && LOAD_PAYLOAD("right")) ||
       ((pressed & BUTTON_LEFT) && LOAD_PAYLOAD("left")) ||
       ((pressed & BUTTON_UP) && LOAD_PAYLOAD("up")) ||
       ((pressed & BUTTON_DOWN) && LOAD_PAYLOAD("down")) ||
       ((pressed & BUTTON_X) && LOAD_PAYLOAD("x")) ||
       ((pressed & BUTTON_Y) && LOAD_PAYLOAD("y")) ||
       ((pressed & BUTTON_R1) && LOAD_PAYLOAD("r")) ||
       ((pressed & BUTTON_SELECT) && LOAD_PAYLOAD("sel")) ||
       LOAD_PAYLOAD("def"))
        ((void (*)())PAYLOAD_ADDRESS)();
}