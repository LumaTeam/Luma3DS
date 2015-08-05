#include <stdint.h>
#include <stddef.h>
#include "fatfs/ff.h"
#include "fatfs/sdmmc/sdmmc.h"
#include "../launcher_path.h"

void *payload_loc = (void *)0x08000000;
unsigned int payload_offset = 0x22000;
unsigned int payload_size = 0x50000;

// This is where the loader writes the framebuffer offsets
struct framebuffers {
    uint32_t *top_left;
    uint32_t *top_right;
    uint32_t *bottom;
} *framebuffers = (struct framebuffers *)0x23FFFE00;

#define screen_top_width 400
#define screen_top_height 240
#define screen_bottom_width 340
#define screen_bottom_height 240
#define screen_top_size (screen_top_width * screen_top_height * 3)
#define screen_bottom_size (screen_bottom_width * screen_bottom_height * 3)

void memset32(void *dest, uint32_t filler, uint32_t size)
{
    uint32_t *dest32 = (uint32_t *)dest;
    for (uint32_t i = 0; i < size / 4; i++) {
        dest32[i] = filler;
    }
}

void clear_screens() {
    memset32(framebuffers->top_left, 0, screen_top_size);
    memset32(framebuffers->top_right, 0, screen_top_size);
    memset32(framebuffers->bottom, 0, screen_bottom_size);
}

// This is supposed to be a small-ish payload that loads a bigger payload.
// This exists because some payloads are too big to be loaded in the memory of some apps.
void main()
{
    FATFS fs;
    FIL handle;
    unsigned int bytes_read;

    // Mount the SD card
    if (f_mount(&fs, "0:", 1) != FR_OK) return;

    // Load the payload
    if (f_open(&handle, "/" LAUNCHER_PATH, FA_READ) != FR_OK) return;
    if (f_lseek(&handle, payload_offset) != FR_OK) return;
    if (f_read(&handle, payload_loc, payload_size, &bytes_read) != FR_OK) return;
    if (f_close(&handle) != FR_OK) return;

    // Unmount the SD card
    if (f_mount(NULL, "0:", 1) != FR_OK) return;

    // This is mostly to adhere to the gateway "standard",
    //   so existing gateway payloads don't have to be modified.
    // Write the framebuffer offsets to the places where gateway expects them
    *(uint32_t **)0x080FFFC0 = framebuffers->top_left;
    *(uint32_t **)0x080FFFC4 = framebuffers->top_right;
    *(uint32_t **)0x080FFFD4 = framebuffers->bottom;
    *(uint32_t *)0x080FFFD8 = 0;  // Select top framebuffer?

    // Loaded correctly. The rest is up to the payload.
    clear_screens();

    // Jump to the payload, right behind the interrupt vector table.
    ((void (*)())(payload_loc + 0x30))();
}
