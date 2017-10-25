#include <3ds.h>
#include "menus/explorer.h"
#include "menus/tools.h"
#include "menus/permissions.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"


extern bool reboot;


const char *strrchr(const char *src, int c)
{
    char *src_c = (char *)src;
    while(*src_c != 0)
    {
        if(*src_c == c) return src_c;
        src_c++;
    }

    return NULL;
}

const char *get_ext(const char *filename) 
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}




void Explorer(void)
{
	
	bool returnhomemenu = false;
	reboot = false;
	
	Dir_Name* Dir = (Dir_Name*)MAP_BASE_1;
	int *prev_index = (int *)0x080FFF00;
	int sDir = 0;
	int index = 0;
	int pos_index = 0;
	int pos = 0;
	
	u32 tmp = 0;
	svcControlMemoryEx(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE, true);
	memcpy(Dir[sDir].Path,"/", 2);
	
	do
	{
		for(int i=0;i<128;i++)
			Dir[0].FoFext[i] = true;
		
		Draw_Lock();
		Draw_ClearFramebuffer();
		Draw_FlushFramebuffer();
		Draw_Unlock();
		
		Handle handle;
		FS_Archive sdmcArchive;
		FS_DirectoryEntry entry;
		FS_Path dirPath = fsMakePath(PATH_ASCII, Dir[sDir].Path);
		FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));
		FSUSER_OpenDirectory(&handle, sdmcArchive, dirPath);
		
		u32 fileRead = 0;
		
		int count = 0;
		do 
		{
			
			FSDIR_Read(handle, &fileRead, 1, (FS_DirectoryEntry*)&entry);
			if (!fileRead) {
				break;
			}
			
			utf16_to_utf8((uint8_t*) Dir[sDir].fileNames[count], entry.name, 512 - 1);
			
			//folder or file
			memcpy(Dir[0].PathDir, Dir[sDir].Path, 512);
			strcat(Dir[0].PathDir, "/");
			strcat(Dir[0].PathDir, Dir[sDir].fileNames[count]);
			
			Handle FoFHandle = 0;
			Result ret = FSUSER_OpenFileDirectly(&FoFHandle, ARCHIVE_SDMC, fsMakePath(PATH_ASCII, ""), fsMakePath(PATH_ASCII, Dir[0].PathDir), FS_OPEN_READ, 0);
			if (R_FAILED(ret)){
				Dir[0].FoFext[count] = false;
			}
			
			FSDIR_Close(FoFHandle);
			svcCloseHandle(FoFHandle);
			count++;
		}
		while(!terminationRequest);
		
		
		do
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
			Draw_DrawString(10, 10, COLOR_TITLE, "Menu Explorer");
			Draw_DrawString(10, 20, COLOR_WHITE, "Press A to install CIA, Press X delete");
			for(int i = 0; i < count; i++)
			{
				Draw_DrawString(50, 40+(i*10), COLOR_BLACK, "                                                  ");
				Draw_DrawFormattedString(50, 40+(i*10), i+pos == index ? COLOR_SEL : COLOR_WHITE,"   %s",Dir[sDir].fileNames[i+pos]);
				if(i+pos == index)Draw_DrawString(50, 40+(i*10),COLOR_SEL, "=>");
				Draw_DrawString( 5, 40+(i*10), COLOR_WHITE,(Dir[0].FoFext[i+pos] == true) ? "File  " : "Folder");
				if(i==15)break;
			}
			Draw_FlushFramebuffer();
			Draw_Unlock();
			
			u32 pressed = waitInputWithTimeout(0);
			if(pressed & BUTTON_A){
				
				if(Dir[0].FoFext[index])
				{
					
					memcpy(Dir[0].PathDir, Dir[sDir].Path, 512);
					strcat(Dir[0].PathDir, "/");
					strcat(Dir[0].PathDir, Dir[sDir].fileNames[index]);
					
					const char * ext = get_ext(Dir[sDir].fileNames[index]);
					
					if(strcmp(ext ,"cia") == 0)
					{
						reboot = true;
						installCIA(Dir[0].PathDir, MEDIATYPE_SD);
					}
					
					continue;
				}
				
				//memory path
				memcpy(Dir[sDir+1].Path, Dir[sDir].Path, 512);
				if(sDir>0)strcat(Dir[sDir+1].Path, "/");
				strcat(Dir[sDir+1].Path, Dir[sDir].fileNames[index]);
				sDir++;
				//memory index previous
				prev_index[pos_index] = index;
				pos_index++;
				index=0;
				
				break;
				
			} else if(pressed & BUTTON_DOWN){ 
				index = (index == count-1) ? 0 : index+1;
			} else if(pressed & BUTTON_UP){
				index = (index == 0) ? count-1 : index-1;
			} else if(pressed & BUTTON_X){
				
				if(ShowUnlockSequence(1))
				{
					memcpy(Dir[0].PathDir, Dir[sDir].Path, 512);
					if(sDir>0)strcat(Dir[0].PathDir, "/");
					strcat(Dir[0].PathDir, Dir[sDir].fileNames[index]);
					
					Result ret = FSUSER_DeleteFile(sdmcArchive, fsMakePath(PATH_ASCII, Dir[0].PathDir));
					if (R_FAILED(ret)){
						ret = FSUSER_DeleteDirectory(sdmcArchive, fsMakePath(PATH_ASCII, Dir[0].PathDir));
						if (R_FAILED(ret)){
							FSUSER_DeleteDirectoryRecursively(sdmcArchive, fsMakePath(PATH_ASCII, Dir[0].PathDir));
						}
					}
						
					index = 0;
					break;
				}
				
			}else if(pressed & BUTTON_B){
				
				
				memcpy(Dir[sDir].Path, "", 2);
				if(sDir == 0)returnhomemenu = true;
				else sDir--;
				
				pos_index--;
				index = prev_index[pos_index];
				
				break;
				
			} 
			if(returnhomemenu)break;
		}
		while(!terminationRequest);
		
		if(returnhomemenu && reboot){
			nsInit();
			NS_TerminateProcessTID(0x0004003000009802ULL);
			nsExit();
		}
		
		if(returnhomemenu)
		{
			FSDIR_Close(handle);
			svcCloseHandle(handle);
			FSUSER_CloseArchive(sdmcArchive);
			svcControlMemory(&tmp, MAP_BASE_1, 0, MAP_BASE_SIZE, MEMOP_FREE, 0);
			return;
		}
		FSDIR_Close(handle);
		svcCloseHandle(handle);
		FSUSER_CloseArchive(sdmcArchive);
		
	}
	while(!terminationRequest);
	

}
