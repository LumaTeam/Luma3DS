#include <3ds.h>
#include "draw.h"
#include "menus/permissions.h"

//thank @D0k3
bool ShowUnlockSequence(u32 seqlvl) 
{
    
	Draw_Lock();
    Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();
	
	const int seqcolors[7] = { COLOR_WHITE, COLOR_GREEN, COLOR_BRIGHTYELLOW,
        COLOR_ORANGE, COLOR_BRIGHTBLUE, COLOR_RED, COLOR_DARKRED };
    const u32 sequences[7][5] = {
        { BUTTON_RIGHT, BUTTON_DOWN, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_A },
        { BUTTON_LEFT, BUTTON_DOWN, BUTTON_RIGHT, BUTTON_UP, BUTTON_A },   
        { BUTTON_LEFT, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_UP, BUTTON_A },
        { BUTTON_LEFT, BUTTON_UP, BUTTON_RIGHT, BUTTON_UP, BUTTON_A },
        { BUTTON_RIGHT, BUTTON_DOWN, BUTTON_LEFT, BUTTON_DOWN, BUTTON_A },
        { BUTTON_DOWN, BUTTON_LEFT, BUTTON_UP, BUTTON_LEFT, BUTTON_A },
        { BUTTON_UP, BUTTON_DOWN, BUTTON_LEFT, BUTTON_RIGHT, BUTTON_A }
    };
    //  '\x18' UP - '\x19' DOWN - '\x1A' RIGHT - '\x1B' LEFT
	const char seqsymbols[7][5] = { 
        { '\x1A', '\x19', '\x1A', '\x19', 'A' },
        { '\x1B', '\x19', '\x1A', '\x18', 'A' },
        { '\x1B', '\x1A', '\x19', '\x18', 'A' },
        { '\x1B', '\x18', '\x1A', '\x18', 'A' },
        { '\x1A', '\x19', '\x1B', '\x19', 'A' },
        { '\x19', '\x1B', '\x18', '\x1B', 'A' },
        { '\x18', '\x19', '\x1B', '\x1A', 'A' }
    };
    const u32 len = 5;
    
	u32 color_off = COLOR_GREY;
    u32 color_on = seqcolors[seqlvl];
    u32 lvl = 0;
	
	Draw_Lock();
    Draw_DrawString(160 - ((24 * 8) / 2), 50, COLOR_WHITE, "enter the sequence below");
	Draw_DrawString(160 - ((21 * 8) / 2), 60, COLOR_WHITE, "or press B to cancel.");
	Draw_DrawString(160 - ((23 * 8) / 2), 80, COLOR_WHITE, "To proceed, enter this:");
    Draw_FlushFramebuffer();
	Draw_Unlock();
    while (true) {
        for (u32 n = 0; n < len; n++) {
            Draw_Lock();
			Draw_DrawFormattedString(66 + (n*4*8), 100, (lvl > n) ? color_on : color_off, "<%c>", seqsymbols[seqlvl][n]);
			Draw_FlushFramebuffer();
			Draw_Unlock();
		}
		
        if (lvl == len)
            break;
        u32 pad_state = waitInputWithTimeout(10000);
        if (!(pad_state & BUTTON_ANY))
            continue;
        else if (pad_state & sequences[seqlvl][lvl])
            lvl++;
        else if (pad_state & BUTTON_B)
            break;
        else if (lvl == 0 || !(pad_state & sequences[seqlvl][lvl-1]))
            lvl = 0;
    }
    
	Draw_Lock();
    Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();
    
    return (lvl >= len);
}