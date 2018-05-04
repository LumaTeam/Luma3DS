//Kasai07


#include <3ds.h>
#include "menus/tools.h"
#include "menus/chainloader.h"
#include "menus/explorer.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"


void bootloader(void)
{
	
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	
	Handle handle = 0;
	FS_Archive sdmcArchive;
	Dir_Name* firm = (Dir_Name*)MAP_BASE_1;
	
	Draw_Lock();
	const FS_Path PathFirm = fsMakePath(PATH_ASCII, "/luma/payloads");
	
	if (FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, NULL)) != 0) {
		Draw_DrawString(10, 60, COLOR_RED, "Could not access SD Card !!!");
		Draw_FlushFramebuffer();
		Draw_Unlock();
		waitInputWithTimeout(0);
		return;
	}
	
	if (FSUSER_OpenDirectory(&handle, sdmcArchive, PathFirm) != 0) {
		Draw_DrawString(10, 60, COLOR_RED, "No directory");
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
	
	u32 fileRead = 0;
	int count = 0;
	while (true) {
		
		FS_DirectoryEntry entry = {};
		FSDIR_Read(handle, &fileRead, 1, &entry);
		if (!fileRead) {
			break;
		}
		utf16_to_utf8((uint8_t*) firm->fileNames[count], entry.name, 512 - 1);
		count++;
	}
	FSDIR_Close(handle);
	svcCloseHandle(handle);
	
	
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
		Draw_DrawString(10, 10, COLOR_TITLE, "Menu ChainLoader");
		Draw_DrawString(10, 30, COLOR_WHITE, "Press A to Loader Firm, press B to return");
		for(int i = 0; i < count; i++)
		{
			Draw_DrawString(30, 60+(i*10), COLOR_BLACK, "                                                ");
			Draw_DrawFormattedString(30, 60+(i*10), (i+pos == index) ? COLOR_SEL : COLOR_WHITE, "   %s",firm->fileNames[i+pos]);
			if(i+pos== index)Draw_DrawString(30, 60+(i*10),COLOR_SEL, "=>");
			if(i==12)break;
		}
		Draw_FlushFramebuffer();
		Draw_Unlock();
		
		u32 pressed = waitInputWithTimeout(0);
		if(pressed & BUTTON_A){
			
			sprintf(firm->Path,  "/luma/payloads/%s", firm->fileNames[index]);
			if(copy_firm_to_buf(sdmcArchive, firm->Path) == 0)svcKernelSetState(7);
			
		} else if(pressed & BUTTON_DOWN){
			index = (index == count-1) ? 0 : index+1;
		} else if(pressed & BUTTON_UP){
			index = (index == 0) ? count-1 : index-1;
		} else if(pressed & BUTTON_B){
			break;
		}
		
	}
	
	FSDIR_Close(handle);
	svcCloseHandle(handle);
	FSUSER_CloseArchive(sdmcArchive);
	svcControlMemory(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_FREE, 0);
	
	return;
    
}



u32 copy_firm_to_buf(FS_Archive sdmcArchive, const char *Path)
{
	
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();
	
	Handle fileHandle, fileHandle2;
	u64 fileSize;
	
	
	Draw_Lock();
	
	FSUSER_DeleteFile(sdmcArchive, fsMakePath(PATH_ASCII, "/bootonce.firm"));
	
	if (FSUSER_OpenFile(&fileHandle, sdmcArchive, fsMakePath(PATH_ASCII, Path), FS_OPEN_READ, 0) != 0) {
		Draw_DrawString(10, 60, COLOR_RED, "error open file !");
		Draw_FlushFramebuffer();
		Draw_Unlock();
		waitInputWithTimeout(0);
		return 1;
	}
	
	if (FSUSER_OpenFile(&fileHandle2, sdmcArchive, fsMakePath(PATH_ASCII, "/bootonce.firm"), FS_OPEN_CREATE | FS_OPEN_WRITE, 0) != 0) {
		Draw_DrawString(10, 60, COLOR_RED, "error open/write file !");
		Draw_FlushFramebuffer();
		Draw_Unlock();
		FSFILE_Close(fileHandle);
		waitInputWithTimeout(0);
		return 1;
	}
	
	FSFILE_GetSize(fileHandle, &fileSize);
	if(!fileSize){
		FSFILE_Close(fileHandle2);
		FSFILE_Close(fileHandle);
		return 1;
	}
	
	u32 bytes;
	u32 read = 	0x10000;
	u8 *firm_buffer = (u8 *)0x08100000;
	
	for (u64 startSize = fileSize; fileSize != 0; fileSize -= read) 
	{
		if (fileSize < read)
			read = fileSize;
			
		FSFILE_Read(fileHandle, &bytes, startSize-fileSize, firm_buffer, read);
		FSFILE_Write(fileHandle2, &bytes, startSize-fileSize, firm_buffer, read, 0);
	}
	
	FSFILE_Close(fileHandle);
	FSFILE_Close(fileHandle2);
	
	Draw_Lock();
	Draw_ClearFramebuffer();
	Draw_FlushFramebuffer();
	Draw_Unlock();
	return 0;
	
}


