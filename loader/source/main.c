#include "types.h"
#include "buttons.h"
#include "screeninit.h"
#include "fatfs/ff.h"

#define PAYLOAD_ADDRESS	0x23F00000

static u32 loadPayload(const char *path){
    FIL payload;
    unsigned int br;
    if(f_open(&payload, path, FA_READ) == FR_OK)
    {
        f_read(&payload, (void*)PAYLOAD_ADDRESS, f_size(&payload), &br);
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

    if(((pressed & BUTTON_B) && loadPayload("/rei/payloads/b.bin")) ||
       ((pressed & BUTTON_X) && loadPayload("/rei/payloads/x.bin")) ||
       ((pressed & BUTTON_Y) && loadPayload("/rei/payloads/y.bin")) ||
       ((pressed & BUTTON_SELECT) && loadPayload("/rei/payloads/select.bin")) ||
       ((pressed & BUTTON_START) && loadPayload("/rei/payloads/start.bin")) ||
       ((pressed & BUTTON_RIGHT) && loadPayload("/rei/payloads/right.bin")) ||
       ((pressed & BUTTON_LEFT) && loadPayload("/rei/payloads/left.bin")) ||
       ((pressed & BUTTON_UP) && loadPayload("/rei/payloads/up.bin")) ||
       ((pressed & BUTTON_DOWN) && loadPayload("/rei/payloads/down.bin")) ||
       loadPayload("/rei/payloads/default.bin")){
        //Determine if screen was already inited
        if(*(vu8 *)0x10141200 == 0x1) initLCD();
        ((void (*)())PAYLOAD_ADDRESS)();
    }
}