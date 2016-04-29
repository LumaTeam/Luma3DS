#include "fatfs/ff.h"

#define PAYLOAD_ADDRESS	0x23F00000

void main(void)
{
    FATFS fs;

    f_mount(&fs, "0:", 1);

    FIL payload;
    unsigned int read;

    f_open(&payload, (char *)0x24F02000, FA_READ);
    f_read(&payload, (void *)PAYLOAD_ADDRESS, f_size(&payload), &read);
    f_close(&payload);

    ((void (*)())PAYLOAD_ADDRESS)();
}