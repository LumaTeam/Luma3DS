#include "types.h"
#include "buttons.h"
#include "fatfs/ff.h"
#include "string.h"

#define PAYLOAD_ADDRESS	0x23F00000

#define SHORT_PATTERN(x) x ".bin"
#define LONG_PATTERN(x) x "_*.bin"

#define BASE_PATH "/aurei/payloads"
#define PAYLOAD_PATH(x) BASE_PATH "/" SHORT_PATTERN(x)
#define LOAD_PAYLOAD(x) loadPayload(SHORT_PATTERN(x), LONG_PATTERN(x))

static u32 loadPath(const char *path)
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

static u32 findFile(const char *pattern)
{
    DIR dp;
    static FILINFO fno;
    FRESULT fr = f_findfirst(&dp, &fno, BASE_PATH, pattern);
    if(!(fr == FR_OK && fno.fname[0])) {
        f_closedir(&dp);
        return 0;
    }
    static char payloadPath[80];
    *payloadPath = '\0';
    strcat(payloadPath, BASE_PATH);
    strcat(payloadPath, "/");
    strcat(payloadPath, fno.fname);
    u32 result = loadPath(payloadPath);
    f_closedir(&dp);
    return result;
}

static u32 loadPayload(const char *short_pattern, const char *long_pattern) 
{
    return findFile(short_pattern) || findFile(long_pattern);
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
       LOAD_PAYLOAD("def") ||
       loadPath(PAYLOAD_PATH("default")))
        ((void (*)())PAYLOAD_ADDRESS)();
}
