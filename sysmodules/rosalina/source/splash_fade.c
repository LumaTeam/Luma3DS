#include <3ds.h>
#include "splash_fade.h"
#include "MyThread.h"
#include "ifile.h"
#include "draw.h"
#include "menu.h"
#include "luma_config.h"

// An "after payloads" splash is left on screen when Luma3DS hands over to the system firmware.
// The system takes the screens over about 1.3s after the SD card becomes accessible, so fade the
// splash out from that point instead of letting it cut to black.

static const u32 splashFadeSteps = 60;
static const s64 splashFadeStepNs = 16 * 1000 * 1000LL; // ~1s in total

static MyThread splashFadeThread;
static u8 CTR_ALIGN(0x1000) splashFadeThreadStack[0x1000];

// A brightness of 0 doesn't turn the backlight off, so only fade down to 1
static inline u32 splashFadeLevel(u32 start, u32 step)
{
    return start == 0 ? 0 : 1 + (start - 1) * (splashFadeSteps - step) / splashFadeSteps;
}

static void splashFadeThreadMain(void)
{
    // Opening a file on the SD card blocks until the card is mounted
    IFile file;
    if (R_SUCCEEDED(IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/config.ini"), FS_OPEN_READ)))
        IFile_Close(&file);

    u32 topStart = LCD_TOP_BRIGHTNESS, botStart = LCD_BOT_BRIGHTNESS;
    u32 top = topStart, bot = botStart;

    for (u32 i = 1; i <= splashFadeSteps; i++)
    {
        // Stop if the system has already taken over the screens
        if (LCD_TOP_BRIGHTNESS != top || LCD_BOT_BRIGHTNESS != bot)
            return;

        top = splashFadeLevel(topStart, i);
        bot = splashFadeLevel(botStart, i);
        LCD_TOP_BRIGHTNESS = top;
        LCD_BOT_BRIGHTNESS = bot;
        svcSleepThread(splashFadeStepNs);
    }

    // Then black the screens out like the system firmware does, until it takes them over
    if (LCD_TOP_BRIGHTNESS == top && LCD_BOT_BRIGHTNESS == bot)
    {
        LCD_TOP_FILLCOLOR = LCD_FILLCOLOR_ENABLE;
        LCD_BOT_FILLCOLOR = LCD_FILLCOLOR_ENABLE;
    }
}

void splashFadeCreateThread(void)
{
    s64 out = 0;
    svcGetSystemInfo(&out, 0x10000, 4); // multiConfig

    // Only when Luma3DS left an "after payloads" splash on screen (the backlights are off otherwise)
    if ((((u32)out >> (2 * SPLASH)) & 3) != 2 || (LCD_TOP_BRIGHTNESS == 0 && LCD_BOT_BRIGHTNESS == 0))
        return;

    if (R_FAILED(MyThread_Create(&splashFadeThread, splashFadeThreadMain, splashFadeThreadStack, sizeof(splashFadeThreadStack), 20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
}
