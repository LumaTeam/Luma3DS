#include "types.h"
#include "buttons.h"
#include "fatfs/ff.h"

#define PAYLOAD_ADDRESS	0x23F00000

#define BASE_PATH "/aurei/payloads/"
#define PAYLOAD_PATH(x) BASE_PATH x ".bin"

static u32 loadPayload(const char *path)
{
    FIL payload;
    unsigned int br;

    if(f_open(&payload, path, FA_READ) == FR_OK)
    {
        f_read(&payload, (void *)PAYLOAD_ADDRESS, f_size(&payload), &br);
        f_close(&payload);

        return 1;
    }

    return 0;
}

void main(void)
{
    FATFS fs;

    f_mount(&fs, "0:", 1);

    //Get pressed buttons
    u32 pressed = HID_PAD;

    if(((pressed & BUTTON_RIGHT) && loadPayload(PAYLOAD_PATH("right"))) ||
       ((pressed & BUTTON_LEFT) && loadPayload(PAYLOAD_PATH("left"))) ||
       ((pressed & BUTTON_UP) && loadPayload(PAYLOAD_PATH("up"))) ||
       ((pressed & BUTTON_DOWN) && loadPayload(PAYLOAD_PATH("down"))) ||
       ((pressed & BUTTON_X) && loadPayload(PAYLOAD_PATH("x"))) ||
       ((pressed & BUTTON_Y) && loadPayload(PAYLOAD_PATH("y"))) ||
       ((pressed & BUTTON_SELECT) && loadPayload(PAYLOAD_PATH("select"))) ||
       ((pressed & BUTTON_R1) && loadPayload(PAYLOAD_PATH("r"))) ||
       loadPayload(PAYLOAD_PATH("default")))
        ((void (*)())PAYLOAD_ADDRESS)();
}
