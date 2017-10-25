//Kasai07


#include <3ds.h>
#include "menus/tools.h"
#include "menus/explorer.h"
#include "menus/permissions.h"
#include "menus/chainloader.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "menus/gsplcd.h"

bool reboot;

Menu MenuOptions = {
    " Menu Tools",
    .nbItems = 4,
    {
        { "Explorer", METHOD, .method = &Explorer },
		{ "ChainLoader", METHOD, .method = &bootloader },
		{ "Install Cia", METHOD, .method = &CIA_menu },
		{ "Delete Title", METHOD, .method = &Delete_Title },
    }
};

void CIA_menu(void)
{
	
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, MAP_BASE_1, 0, 0x6000, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	
	Dir_Name* cianame = (Dir_Name*)MAP_BASE_1;
	bool quit = true;
	reboot = false;
	
	Handle handle = 0;
	FS_Archive sdmcArchive;
	while (true) 
	{
		
		Draw_Lock();
		const FS_Path PathCia = fsMakePath(PATH_ASCII, "/cias");
		
		if (FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL)) != 0) {
			Draw_DrawString(10, 60, COLOR_RED, "Could not access SD Card !!!");
			Draw_FlushFramebuffer();
			Draw_Unlock();
			waitInputWithTimeout(0);
			return;
		}
		
		if (FSUSER_OpenDirectory(&handle, sdmcArchive, PathCia) != 0) {
			Draw_DrawString(10, 60, COLOR_RED, "No /cias directory");
			Draw_FlushFramebuffer();
			Draw_Unlock();
			waitInputWithTimeout(0);
			return;
		}
		Draw_FlushFramebuffer();
		Draw_Unlock();
		
		Draw_Lock();
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		
		quit = true;
		u32 fileRead = 0;
		int count = 0;
		while (true) {
			
			FS_DirectoryEntry entry = {};
			FSDIR_Read(handle, &fileRead, 1, &entry);
			if (!fileRead) {
				break;
			}
			utf16_to_utf8((uint8_t*) cianame->fileNames[count], entry.name, 512 - 1);
			count++;
		}
		FSDIR_Close(handle);
		svcCloseHandle(handle);
		
		//_============================
		
		
		  
		int index = 0;
		int pos = 0;
		while(true)
		{
			
			if(index==0)pos=0;
			if(count>13)
			{
				if((pos+12)<index){
					pos++; 
					
				}
				if(pos>index)pos--;
				if(index==count-1)pos=count-13;
			}
			
			Draw_Lock();
			Draw_DrawString(10, 10, COLOR_TITLE, "CIA Install Menu");
			Draw_DrawString(10, 30, COLOR_WHITE, "Press A to install, press B to return");
			Draw_DrawString(10, 40, COLOR_WHITE, "Press X delete");
			for(int i = 0; i < count; i++)
			{
				Draw_DrawString(30, 60+(i*10), COLOR_BLACK, "                                                ");
				Draw_DrawFormattedString(30, 60+(i*10), (i+pos == index) ? COLOR_SEL : COLOR_WHITE, "   %s",cianame->fileNames[i+pos]);
				if(i+pos== index)Draw_DrawString(30, 60+(i*10),COLOR_SEL, "=>");
				if(i==12)break;
			}
			Draw_FlushFramebuffer();
			Draw_Unlock();
			
			u32 pressed = waitInputWithTimeout(0);
			if(pressed & BUTTON_A){
				
				reboot = true;
				sprintf(cianame->Path,  "/cias/%s", cianame->fileNames[index]);
				installCIA(cianame->Path, MEDIATYPE_SD);
				
				
			} else if(pressed & BUTTON_DOWN){
				index = (index == count-1) ? 0 : index+1;
			} else if(pressed & BUTTON_UP){
				index = (index == 0) ? count-1 : index-1;
			} else if(pressed & BUTTON_X){
				
				if(ShowUnlockSequence(1))
				{
					sprintf(cianame->Path,  "/cias/%s", cianame->fileNames[index]);
					FSUSER_DeleteFile(sdmcArchive, fsMakePath(PATH_ASCII, cianame->Path));
					
					quit = false;
					break;
				}
				
			}else if(pressed & BUTTON_B){
				quit = true;
				break;
			}
			
		}
		FSUSER_CloseArchive(sdmcArchive);
		if(quit)break;
	}
	
	if(reboot){
		
		nsInit();
		NS_TerminateProcessTID(0x0004003000009802ULL);
		nsExit();
	}	
	
	FSDIR_Close(handle);
	svcCloseHandle(handle);
	FSUSER_CloseArchive(sdmcArchive);
	svcControlMemory(&tmp, MAP_BASE_1, 0, 0x6000, MEMOP_FREE, 0);

	return;
    
}

char* get_size_units(u64 size) {
    if(size > 1024 * 1024 * 1024) {
        return "GB     ";
    }

    if(size > 1024 * 1024) {
        return "MB     ";
    }

    if(size > 1024) {
        return "KB     ";
    }

    return "B     ";
}

double get_size(u64 size) {
    double s = size;
    if(s > 1024) {
        s /= 1024;
    }

    if(s > 1024) {
        s /= 1024;
    }

    if(s > 1024) {
        s /= 1024;
    }

    return s;
}

char* get_eta(u64 seconds) {
    static char disp[12];

    u8 hours     = seconds / 3600;
    seconds     -= hours * 3600;
    u8 minutes   = seconds / 60;
    seconds     -= minutes* 60;
	
	
	sprintf(disp, "%02uh%02um%02us",hours,minutes,(u8) seconds);

    return disp;
}

Result installCIA(const char *path, FS_MediaType media)
{
	amInit();
   
	bool* available = false;
	AM_QueryAvailableExternalTitleDatabase(available);
	if(!available)AM_InitializeExternalTitleDatabase(false);
	
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, MAP_BASE_2, 0, MAP_BASE_SIZE, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	char *cia_buffer = (char *)0x10000000;
	AM_TitleEntry* title = (AM_TitleEntry*)0x10100000;
	
	Draw_Lock();
    Draw_ClearFramebuffer();
	
	u64 size = 0;
	u32 bytes;
	Handle ciaHandle = 0;
	Handle fileHandle = 0;
	
	Draw_DrawFormattedString(10, 10, COLOR_TITLE, "Open File %s.",path);
	Result ret = FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SDMC, fsMakePath(PATH_ASCII, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);
	if (R_FAILED(ret)){
		goto error;
	}
	ret = AM_GetCiaFileInfo(media, title, fileHandle);
	if (R_FAILED(ret)){
		goto error;
	}
	
	ret = FSFILE_GetSize(fileHandle, &size);
	if (R_FAILED(ret)){
		goto error;
	}
	
	Draw_DrawString(10, 20, COLOR_WHITE, "Waiting for CIA installation...");
	Draw_DrawString(10, 30, COLOR_WHITE, "Press B to cancel...");
	
	ret = AM_StartCiaInstall(media, &ciaHandle);
	if (R_FAILED(ret)){
		goto error;
	}
	
	u64 timer0 = svcGetSystemTick()/TICKS_PER_MSEC/1000;
	
	u64 i = 0;
	u32 read = 	0x100000;
	for (u64 startSize = size; size != 0; size -= read) 
	{
		if (size < read)
			read = size;
			
		u64 timer1 = (svcGetSystemTick()/TICKS_PER_MSEC/1000)-timer0;
		char* units = get_size_units(size);
		u64 getsize = get_size(size);
		
		Draw_DrawFormattedString(10, 50, COLOR_WHITE, "%3llu%%", (i * 100) / startSize);
		Draw_DrawFormattedString(50, 50, COLOR_WHITE, "remaining: %llu %s", getsize, units);
		Draw_DrawFormattedString(170, 50, COLOR_WHITE, "%s", get_eta(timer1));
		
		i+=read;
		
		if(HID_PAD & BUTTON_B)
		{
			ret = AM_CancelCIAInstall (ciaHandle);
			goto error;
		}
		FSFILE_Read(fileHandle, &bytes, startSize-size, cia_buffer, read);
		FSFILE_Write(ciaHandle, &bytes, startSize-size, cia_buffer, read, 0);
	}
	
	svcControlMemory(&tmp, MAP_BASE_2, 0, MAP_BASE_SIZE, MEMOP_FREE, 0);
	
	ret = AM_FinishCiaInstall(ciaHandle);
	if (R_FAILED(ret)){
		goto error;
	}
	ret = svcCloseHandle(fileHandle);
	if (R_FAILED(ret)){
		goto error;
	}
	
	Draw_DrawString(10, 50, COLOR_GREEN, "100%");
	Draw_DrawString(10, 70, COLOR_GREEN, "CIA installation successful.");
	Draw_FlushFramebuffer();
	Draw_Unlock();
	
	waitInputWithTimeout(0);
	
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();

	FSDIR_Close(ciaHandle);
	svcCloseHandle(ciaHandle);
	FSDIR_Close(fileHandle);
	svcCloseHandle(fileHandle);
	amExit();
	return 0;
	
	error:
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		amExit();
		return ret;
}

void Delete_Title(void)
{
	amInit();
	reboot = false;
	bool reboothomemenu = false;
	
	u32 tmp = 0;
	
	svcControlMemoryEx(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	
	AM_TitleEntry* title = (AM_TitleEntry*)0x08006000;
	u64 *titleIds = NULL;
	titleIds = (u64 *)0x08008000;
	
	Info_Title* info = (Info_Title*)0x08010000;
	
	while(true)
	{
		
		
		u32 titleCount = 0;
		AM_GetTitleCount(MEDIATYPE_SD, &titleCount);
		
		u32 titleRead = 0;
		AM_GetTitleList(&titleRead, MEDIATYPE_SD, titleCount, titleIds);
		
		
		for(u32 i = 0; i < titleCount; i++)
		{
			memset(info[i].shortDescription, 0,0x40);
			memset(info[i].productCode, 0,0x10);
			get_Name_TitleID(titleIds[i], i, info);
			AM_GetTitleInfo(MEDIATYPE_SD, titleCount, &titleIds[i], &title[i]);
			AM_GetTitleProductCode(MEDIATYPE_SD, title[i].titleID, info[i].productCode);
		}	
		
		u32 index = 0;
		u32 pos = 0;
		while(true)
		{
			if(index==0)pos=0;
			if(titleCount>11)
			{
				if((pos+10)<index){
					pos++; 
					
				}
				if(pos>index)pos--;
				if(index==titleCount-1)pos=titleCount-11;
			}
			
			/*
			for(int y = 0; y < 240; y++)
			{	 
				for(int x = 0; x < 320; x++)	
				{
					//u32 screenPos = (posX * 240 * 2 + (240 - x - posY -1) * 2) + y * 2 * 240;
					u32 screenPos = (0 * 240 * 2 + (240 - x - 0 -1) * 2) + y * 2 * 240;
					*((u8*)FB_BOTTOM_VRAM_ADDR + screenPos++) = 0x30;
					*((u8*)FB_BOTTOM_VRAM_ADDR + screenPos++) = 0x30;
				}	
			}*/
			
			Draw_Lock();
			Draw_DrawString(10, 10, COLOR_TITLE, "Menu Delete Title");
			Draw_DrawFormattedString(80, 180, COLOR_WHITE, "Title ID : %s                              ",info[index].productCode);
			Draw_DrawFormattedString(80, 195, COLOR_WHITE, "Publicher: %s                              ",info[index].publisher);
			for(u32 i = 0; i < titleCount; i++)
			{
				Draw_DrawString(10, 40+(i*10), COLOR_BLACK, "                                                  ");
				Draw_DrawFormattedString(10, 40+(i*10), i+pos == index ? COLOR_TITLE : COLOR_WHITE, "   %s", info[i+pos].shortDescription);
				if(i+pos == index)Draw_DrawString(10, 40+(i*10),COLOR_TITLE, "=>");
				if(i==10)break;
			}
			
			
			//=======SDMH======
			
			u8 IconPixel[0x1200];
			u8 OrdrePixel[64] = {
			0,  1,  8,  9,  2,  3,  10, 11, 16, 17, 24, 25, 18, 19, 26, 27,
			4,  5,  12, 13, 6,  7,  14, 15, 20, 21, 28, 29, 22, 23, 30, 31,
			32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59,
			36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63 };
			int count = 0;
			for( int y = 0; y < 48; y += 8)
			{
				for(int x = 0; x < 48; x += 8)
				{
					for(int k = 0; k < 64; k++)
					{
						u8 xx =  (OrdrePixel[k] & 0x7);
						u8 yy =  (OrdrePixel[k] >> 3);
						IconPixel[(((x + xx) * 48 + (y + yy)) * 2)+0] = info[index].largeIcon[count++];
						IconPixel[(((x + xx) * 48 + (y + yy)) * 2)+1] = info[index].largeIcon[count++];
					}
				}
			}
			
			int pixel=0;
			for(int y = 0; y < 48; y++)
			{	 
				for(int x = 0; x < 48; x++)	
				{
					//u32 screenPos = (posX * 240 * 2 + (240 - x - posY -1) * 2) + y * 2 * 240;
					u32 screenPos = (20 * 240 * 2 + (240 - x - 170 -1) * 2) + y * 2 * 240;
					*((u8*)FB_BOTTOM_VRAM_ADDR + screenPos++) = IconPixel[pixel++];
					*((u8*)FB_BOTTOM_VRAM_ADDR + screenPos++) = IconPixel[pixel++];
				}	
			}
			//====================
			
			Draw_FlushFramebuffer();
			Draw_Unlock();
			
			u32 pressed = waitInputWithTimeout(0);
			if(pressed & BUTTON_A){
				
				if(ShowUnlockSequence(1))
				{
					reboot=true;
					Draw_Lock();
					Draw_ClearFramebuffer();
					Draw_DrawFormattedString(10, 10, COLOR_WHITE, "Delete Title %s.",info[index].shortDescription);
					AM_DeleteTitle(MEDIATYPE_SD, title[index].titleID);
					Draw_DrawString(10, 30, COLOR_GREEN, "Delete Title successfully...");
					Draw_FlushFramebuffer();
					Draw_Unlock();
					
					waitInputWithTimeout(0);
					
					Draw_Lock();
					Draw_ClearFramebuffer();
					Draw_FlushFramebuffer();
					Draw_Unlock();
					break;
				}
				
			} else if(pressed & BUTTON_DOWN){ 
				index = (index == titleCount-1) ? 0 : index+1;
			} else if(pressed & BUTTON_UP){
				index = (index == 0) ? titleCount-1 : index-1;
			} else if(pressed & BUTTON_B){
				reboothomemenu=true;
				break;
			} 
		}
		
		if(reboothomemenu)break;
	}
	
	if(reboot){
		
		nsInit();
		NS_TerminateProcessTID(0x0004003000009802ULL);
		nsExit();
	}	
	titleIds = NULL;
	svcControlMemory(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_FREE, 0);
	
	return;
	
}

static CFG_Language region_default_language[] = {
        CFG_LANGUAGE_JP,
		CFG_LANGUAGE_EN,
        CFG_LANGUAGE_EN,
        CFG_LANGUAGE_EN,
        CFG_LANGUAGE_ZH,
        CFG_LANGUAGE_KO,
        CFG_LANGUAGE_ZH
};

bool string_empty(const char* str) {
    if(strlen(str) == 0) {
        return true;
    }

    const char* curr = str;
    while(*curr) {
        if(*curr != ' ') {
            return false;
        }

        curr++;
    }

    return true;
}
SMDH_title* select_smdh_title(SMDH* smdh) {
    char shortDescription[0x100] = {'\0'};

    CFG_Language systemLanguage;
    
	if(R_SUCCEEDED(CFGU_GetSystemLanguage((u8*) &systemLanguage))) {
        utf16_to_utf8((uint8_t*) shortDescription, smdh->titles[systemLanguage].shortDescription, sizeof(shortDescription) - 1);
	}
	
    if(string_empty(shortDescription)) {
        CFG_Region systemRegion;
        if(R_SUCCEEDED(CFGU_SecureInfoGetRegion((u8*) &systemRegion))) {
            systemLanguage = region_default_language[systemRegion];
        } else {
            systemLanguage = CFG_LANGUAGE_JP;
        }
    }

    return &smdh->titles[systemLanguage];
}
void get_Name_TitleID(u64 titleId, u32 count, Info_Title* info)
{
	
	cfguInit();
	Result res = 0;
	
	static const u32 filePath[5] = {0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000};
	u32 archivePath[4] = {(u32) (titleId & 0xFFFFFFFF), (u32) ((titleId >> 32) & 0xFFFFFFFF), MEDIATYPE_SD, 0x00000000};
	
	FS_Path path = {PATH_BINARY, sizeof(archivePath), archivePath};
	FS_Path path2 = {PATH_BINARY, sizeof(filePath), filePath};
	
	Handle fileHandle;
	if(R_SUCCEEDED(FSUSER_OpenFileDirectly(&fileHandle, ARCHIVE_SAVEDATA_AND_CONTENT, path, path2, FS_OPEN_READ, 0))) {
		SMDH* smdh = (SMDH*)0x08000000;
		u32 bytesRead = 0;
		if(R_SUCCEEDED(res = FSFILE_Read(fileHandle, &bytesRead, 0, smdh, sizeof(SMDH))) && bytesRead == sizeof(SMDH)) {
			if(smdh->magic[0] == 'S' && smdh->magic[1] == 'M' && smdh->magic[2] == 'D' && smdh->magic[3] == 'H') {
				
				SMDH_title* smdhTitle = (SMDH_title*)0x08004000;
				smdhTitle = select_smdh_title(smdh);
				
				utf16_to_utf8((uint8_t*) info[count].shortDescription, smdhTitle->shortDescription, sizeof(info[count].shortDescription) - 1);
				//utf16_to_utf8((uint8_t*) info[count].longDescription, smdhTitle->longDescription, sizeof(info[count].longDescription) - 1);
				utf16_to_utf8((uint8_t*) info[count].publisher, smdhTitle->publisher, sizeof(info[count].publisher) - 1);
				memcpy(info[count].largeIcon, smdh->largeIcon, 0x1200);
				
			} 
		}
		
	}
	FSDIR_Close(fileHandle);
	svcCloseHandle(fileHandle);
	cfguExit();
	
}

typedef struct 
{
	char signature[2];
	int size;				
	int rsv;				
	int offsetim;			
	int size_imhead;		 
	
	int width;			
	int height;				
	
	short nbplans; 			// toujours 1
	short bpp;				//bit Pixel
	int compression;		
	int sizeim;				
	int hres;				
	int vres;				
	int cpalette;			// color palette
	int cIpalette;			
	
	u8 pixel24[400*240*3];
	u8 pixel16[400*240*2];
	
} __attribute__((packed)) BMP_Data;

void drawimage(void)
{
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, 0x10000000, 0, 0x200000, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	
	BMP_Data* bmp = (BMP_Data*)0x10100000;
	
	FS_Archive sdmcArchive;
	Handle fileHandle;
	u64 fileSize;
	
	FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL));
	FSUSER_OpenFile(&fileHandle, sdmcArchive, fsMakePath(PATH_ASCII, "/bg.bmp"), FS_OPEN_READ, 0);
	FSFILE_GetSize(fileHandle, &fileSize);
	
	Draw_DrawFormattedString(0, 0, COLOR_WHITE, "%d",fileSize);
	waitInputWithTimeout(0);
	
	u32 bytes;
	u32 read = 	10000;
	u8 *buffer = (u8 *)0x10000000;
	
	for (u64 start = fileSize; fileSize != 0; fileSize -= read) 
	{
		if (fileSize < read)
			read = fileSize;
			
		FSFILE_Read(fileHandle, &bytes, start-fileSize, buffer, read);
		
	}
	memcpy(bmp, buffer, sizeof(BMP_Data*));
	
	u32 size_pixel = bmp->width*bmp->height;
	
	Draw_DrawFormattedString(10, 100, COLOR_WHITE, "%d",buffer);
	Draw_DrawFormattedString(10, 110, COLOR_WHITE, "%d",bmp->height);
	Draw_DrawFormattedString(10, 120, COLOR_WHITE, "%d",bmp->bpp);
	Draw_DrawFormattedString(10, 130, COLOR_WHITE, "%d",size_pixel);
	
	waitInputWithTimeout(0);
	
	int dir = 0;
	for(u32 i = 0; i > size_pixel; i +=3)
	{
		u8 red   = bmp->pixel24[i+0];
		u8 green = bmp->pixel24[i+1];
		u8 blue  = bmp->pixel24[i+2];
		
		u16 b = (blue >> 3) & 0x1f;
		u16 g = ((green >> 2) & 0x3f) << 5;
		u16 r =  ((red >> 3)& 0x1f) << 11;
		
		bmp->pixel16[dir++] = (r|g|b);
	}
	
	FSFILE_Close(fileHandle);
	FSUSER_CloseArchive(sdmcArchive);
	
	dir = 0;
	for(int y = 0; y < 240; y++)
	{	 
		for(int x = 0; x < 320; x++)	
		{
			//u32 screenPos = (posX * 240 * 2 + (240 - x - posY -1) * 2) + y * 2 * 240;
			u32 screenPos = (0 * 240 + (240 - x - 0 -1)) + y * 240;
			*((u16*)FB_BOTTOM_VRAM_ADDR + screenPos++) = bmp->pixel16[dir++];
		}	
	}
	
	svcControlMemory(&tmp, 0x10000000, 0, 0x200000, MEMOP_FREE, 0);
	return;
}	