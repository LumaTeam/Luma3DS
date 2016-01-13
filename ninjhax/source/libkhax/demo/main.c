#ifndef LIBKHAX_AS_LIB

#include <3ds.h>
#include <3ds/services/am.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "../khax.h"

#define KHAX_lengthof(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))

s32 g_backdoorResult = -1;

s32 dump_chunk_wrapper()
{
	__asm__ volatile("cpsid aif");
	g_backdoorResult = 0x6666abcd;
	return 0;
}

// Test access to "am" service, which we shouldn't have access to, unless khax succeeds.
Result test_am_access_inner(char *productCode)
{
	// Title IDs of "mset" in the six regions
	static const u64 s_msetTitleIDs[] =
	{
		0x0004001000020000, 0x0004001000021000, 0x0004001000022000,
		0x0004001000026000, 0x0004001000027000, 0x0004001000028000
	};
	Result result;
	char productCodeTemp[16 + 1];
	unsigned x;

	// Initialize "am"
	result = amInit();
	if (result != 0)
	{
		return result;
	}

	// Check for the existence of the title IDs.
	for (x = 0; x < KHAX_lengthof(s_msetTitleIDs); ++x)
	{
		result = AM_GetTitleProductCode(0, s_msetTitleIDs[x], productCodeTemp);
		if (result == 0)
		{
			memcpy(productCode, productCodeTemp, sizeof(productCodeTemp));
			amExit();
			return 0;
		}
	}

	amExit();
	return -1;
}

// Self-contained test.
void test_am_access_outer(int testNumber)
{
	char productCode[16 + 1];
	Result result = test_am_access_inner(productCode);
	if (result != 0)
	{
		productCode[0] = '\0';
	}
	printf("amtest%d:%08lx %s\n", testNumber, result, productCode);
}


int main()
{
	// Initialize services
/*	srvInit();			// mandatory
	aptInit();			// mandatory
	hidInit(NULL);	// input (buttons, screen)*/
	gfxInitDefault();			// graphics
/*	fsInit();
	sdmcInit();
	hbInit();
	qtmInit();*/

	consoleInit(GFX_BOTTOM, NULL);

	consoleClear();

	test_am_access_outer(1); // test before libkhax

	Result result = khaxInit();
	printf("khaxInit returned %08lx\n", result);

	printf("backdoor returned %08lx\n", (svcBackdoor(dump_chunk_wrapper), g_backdoorResult));

	test_am_access_outer(2); // test after libkhax

	printf("khax demo main finished\n");
	printf("Press X to exit\n");

	khaxExit();

	while (aptMainLoop())
	{
		// Wait next screen refresh
		gspWaitForVBlank();

		// Read which buttons are currently pressed 
		hidScanInput();
		u32 kDown = hidKeysDown();
		(void) kDown;
		u32 kHeld = hidKeysHeld();
		(void) kHeld;

		// If START is pressed, break loop and quit
		if (kDown & KEY_X){
			break;
		}

		//consoleClear();

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	// Exit services
/*	qtmExit();
	hbExit();
	sdmcExit();
	fsExit();*/
	gfxExit();
/*	hidExit();
	aptExit();
	srvExit();*/

	// Return to hbmenu
	return 0;
}

#endif // LIBKHAX_AS_LIB
