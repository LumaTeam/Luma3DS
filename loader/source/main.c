#include "types.h"
#include "buttons.h"
#include "fatfs/ff.h"

#define PAYLOAD_ADDRESS	0x23F00000

static u32 loadPayload(const char *path){
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

void main(void){
    FATFS fs;
    f_mount(&fs, "0:", 1);

    //Get pressed buttons
    u16 pressed = HID_PAD;

    if(((pressed & BUTTON_B) && loadPayload("/aurei/payloads/b.bin")) ||
       ((pressed & BUTTON_X) && loadPayload("/aurei/payloads/x.bin")) ||
       ((pressed & BUTTON_Y) && loadPayload("/aurei/payloads/y.bin")) ||
       ((pressed & BUTTON_SELECT) && loadPayload("/aurei/payloads/select.bin")) ||
       ((pressed & BUTTON_START) && loadPayload("/aurei/payloads/start.bin")) ||
       ((pressed & BUTTON_RIGHT) && loadPayload("/aurei/payloads/right.bin")) ||
       ((pressed & BUTTON_LEFT) && loadPayload("/aurei/payloads/left.bin")) ||
       ((pressed & BUTTON_UP) && loadPayload("/aurei/payloads/up.bin")) ||
       ((pressed & BUTTON_DOWN) && loadPayload("/aurei/payloads/down.bin")) ||
       loadPayload("/aurei/payloads/default.bin"))
        ((void (*)())PAYLOAD_ADDRESS)();
}