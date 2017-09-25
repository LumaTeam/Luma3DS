//Kasai07


#include <3ds.h>
#include "menus/tools.h"
#include "menus/explorer.h"
#include "menus/permissions.h"
#include "mcu.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "ifile.h"

#define MAP_BASE_1		0x08000000
#define MAP_BASE_2 		0x10000000
#define MAP_BASE_SIZE	  0x200000

#define MEMOP_FREE 		1
#define MEMOP_ALLOC 	3
	
#define MEMPERM_READ 	1
#define MEMPERM_WRITE 	2

#define TICKS_PER_MSEC 268111.856



bool reboot;

Menu MenuOptions = {
    " Menu Tools",
    .nbItems = 2,
    {
        { "Explorer", METHOD, .method = &Explorer },
		{ "Install Cia", METHOD, .method = &CIA_menu },
		//{ "Delete Title", METHOD, .method = &Delete_Title },
    }
};

void CIA_menu(void)
{
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, MAP_BASE_1, 0, 0x6000, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	
	Dir_Name* cianame = (Dir_Name*)MAP_BASE_1;
	reboot = false;
	//char cianame[128][128];
	Draw_Lock();
    Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();
	//===================================
	
	Draw_Lock();
	
	FS_Archive sdmcArchive;
	const FS_Path PathCia = fsMakePath(PATH_ASCII, "/cias");
	Handle handle = 0;
	
	if (FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL)) != 0) {
		Draw_DrawString(10, 60, COLOR_RED, "Could not access SD Card !!!");
		waitInputWithTimeout(1000);
		return;
	}
	
	if (FSUSER_OpenDirectory(&handle, sdmcArchive, PathCia) != 0) {
		Draw_DrawString(10, 60, COLOR_RED, "No /cias directory");
		waitInputWithTimeout(1000);
		return;
	}
	Draw_FlushFramebuffer();
	Draw_Unlock();
	
	u32 fileRead = 0;
	int count = 0;
	while (true) {
		
		FS_DirectoryEntry entry = {};
		FSDIR_Read(handle, &fileRead, 1, &entry);
		if (!fileRead) {
			break;
		}
		
		char name[262] = { 0 };
		for (size_t i = 0; i < 262; i++) {
			name[i] = entry.name[i] % 0xff;
		}
		 
		memcpy(cianame->fileNames[count], name, 128);
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
		if(count>16)
		{
			if((pos+15)<index){
				pos++; 
				
			}
			if(pos>index)pos--;
			if(index==count-1)pos=count-16;
		}
		
		Draw_Lock();
		Draw_DrawString(10, 10, COLOR_TITLE, "Menu Install Cia");
		Draw_DrawString(10, 30, COLOR_WHITE, "Press A to install, press B to return");
		for(int i = 0; i < count; i++)
		{
			Draw_DrawString(30, 60+(i*10), COLOR_BLACK, "                                                  ");
			Draw_DrawFormattedString(30, 60+(i*10), (i == index) ? COLOR_SEL : COLOR_WHITE, "   %s",cianame->fileNames[i+pos]);
			if(i == index)Draw_DrawString(30, 60+(i*10),COLOR_SEL, "=>");
		}
		Draw_FlushFramebuffer();
		Draw_Unlock();
		
		u32 pressed = waitInputWithTimeout(1000);
		if(pressed & BUTTON_A){
			
			reboot = true;
			memcpy(cianame->Path, "/cias/",7);
			strcat(cianame->Path, cianame->fileNames[index]);
			installCIA(cianame->Path, MEDIATYPE_SD);
			
			
		} else if(pressed & BUTTON_DOWN){
			index = (index == count-1) ? 0 : index+1;
		} else if(pressed & BUTTON_UP){
			index = (index == 0) ? count-1 : index-1;
		} else if(pressed & BUTTON_B){
			break;
		}
		
		
	}
	FSUSER_CloseArchive(sdmcArchive);
	if(reboot)svcKernelSetState(7);
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
    AM_InitializeExternalTitleDatabase(false);
	
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
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return ret;
	}
	ret = AM_GetCiaFileInfo(media, title, fileHandle);
	if (R_FAILED(ret)){
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return ret;
	}
	
	
	ret = FSFILE_GetSize(fileHandle, &size);
	if (R_FAILED(ret)){
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return ret;
	}
	
	Draw_DrawString(10, 30, COLOR_WHITE, "Wait Cia Install...");
	Draw_DrawString(10, 30, COLOR_WHITE, "Press B to canceled...");
	
	//AM_QueryAvailableExternalTitleDatabase(NULL);
	ret = AM_StartCiaInstall(media, &ciaHandle);
	if (R_FAILED(ret)){
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return ret;
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
			AM_CancelCIAInstall (ciaHandle);
			Draw_ClearFramebuffer();
			Draw_FlushFramebuffer();
			Draw_Unlock();
			reboot = false;
			FSDIR_Close(ciaHandle);
			svcCloseHandle(ciaHandle);
			FSDIR_Close(fileHandle);
			svcCloseHandle(fileHandle);
			return 1;
		}
		FSFILE_Read(fileHandle, &bytes, startSize-size, cia_buffer, read);
		FSFILE_Write(ciaHandle, &bytes, startSize-size, cia_buffer, read, 0);
	}
	
	svcControlMemory(&tmp, MAP_BASE_2, 0, MAP_BASE_SIZE, MEMOP_FREE, 0);
	
	ret = AM_FinishCiaInstall(ciaHandle);
	if (R_FAILED(ret)){
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return ret;
	}
	ret = svcCloseHandle(fileHandle);
	if (R_FAILED(ret)){
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		reboot = false;
		FSDIR_Close(ciaHandle);
		svcCloseHandle(ciaHandle);
		FSDIR_Close(fileHandle);
		svcCloseHandle(fileHandle);
		return ret;
	}
	
	Draw_DrawString(10, 50, COLOR_GREEN, "100%");
	Draw_DrawString(10, 70, COLOR_GREEN, "Installation Cia successfully...");
	Draw_FlushFramebuffer();
	Draw_Unlock();
	
	waitInputWithTimeout(100000);
	
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();

	FSDIR_Close(ciaHandle);
	svcCloseHandle(ciaHandle);
	FSDIR_Close(fileHandle);
	svcCloseHandle(fileHandle);
	
	return 0;
}

/*void Delete_Title(void)
{
	amInit();
	reboot = false;
	bool reboothomemenu = false;
	
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	svcControlMemory(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_FREE, 0);
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
			
			
			Draw_Lock();
			Draw_DrawString(10, 10, COLOR_TITLE, "Menu Delete Title");
			Draw_DrawFormattedString(10, 20, COLOR_WHITE, "Title ID [%s]          ",info[index].productCode);
			for(u32 i = 0; i < titleCount; i++)
			{
				Draw_DrawFormattedString(10, 40+(i*10), i+pos == index ? COLOR_TITLE : COLOR_WHITE, "   %s", info[i+pos].shortDescription);
				if(i == index)Draw_DrawString(10, 40+(i*10),COLOR_TITLE, "=>");
				if(i==10)break;
			}
			Draw_FlushFramebuffer();
			Draw_Unlock();
			
			u32 pressed = waitInputWithTimeout(1000);
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
					
					waitInputWithTimeout(10000);
					
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
	
	if(reboot)svcKernelSetState(7);
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
				memcpy(info[count].largeIcon, smdh->largeIcon, 0x1200);
				
			} 
		}
		
	}
	FSDIR_Close(fileHandle);
	svcCloseHandle(fileHandle);
	cfguExit();
	
}
*/
