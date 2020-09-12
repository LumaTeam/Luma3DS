#include <stdlib.h>
#include <3ds.h>
#include "fmt.h"
#include "draw.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "menus/twlbg_switcher.h"

typedef struct Entry {
    char name[FILE_NAME_MAX];       // File name
    char ext[4];        // File extension
} Entry;

// Use a global to avoid stack overflow, those structs are quite heavy (advice from Nanquitas plugin loader)
static FS_DirectoryEntry   g_entries[MAX_FILES];
static Entry g_displayEntries[MAX_FILES];

static const char *g_dirPath = "/luma/sysmodules";
static const char *g_textFileName = "/luma/sysmodules/twlbgName.txt";
static const char *g_twlbgFixedName = "TwlBg.cxi";
static const char *g_twlbgFailName = "Unknown.cxi";

static char g_twlbgName[FILE_NAME_MAX];

static char g_menuDisplay[266];

u32 entryCount = 0;

void TwlbgSwitcher_DisplayFiles(void)
{   
	Result r = TwlbgSwitcher_PopulateFiles();
    TwlbgSwitcher_UpdateStatus();

	Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

	if (entryCount == 0)
    {
        do
        {
            Draw_Lock();
            Draw_DrawString(10, 10, COLOR_TITLE, "TwlBg Quick Switcher - by Nutez");

            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "No files found");

            Draw_FlushFramebuffer();
            Draw_Unlock();
        } while (!(waitInput() & KEY_B) && !menuShouldExit);
    }
    else
    {
        Entry * displayEntries = g_displayEntries;
        u32 selected = 0;
        do
        {
            Draw_Lock();
            if (R_FAILED(r))
            {
                Draw_ClearFramebuffer();
            }
            if (R_SUCCEEDED(r))
            {
                Draw_DrawFormattedString(10, 10, COLOR_TITLE, "TwlBg Quick Switcher - by Nutez");

                for (u32 i = 0; i < entryCount; i++)
                {
                    Draw_DrawString(30, 30 + i * SPACING_Y, ((!strncasecmp(displayEntries[i].ext, "cxi", 3)) && !(!strncasecmp(displayEntries[i].name, g_twlbgFixedName, 10))) ? COLOR_WHITE : COLOR_RED, displayEntries[i].name);
                    Draw_DrawCharacter(10, 30 + i * SPACING_Y, COLOR_TITLE, i == selected ? '>' : ' ');
                }
            }
            else
            {
                Draw_DrawFormattedString(10, 10, COLOR_TITLE, "ERROR: %08lx", r);
            }
            Draw_FlushFramebuffer();
            Draw_Unlock();

            if (menuShouldExit) break;

            u32 pressed;
            do
            {
                pressed = waitInputWithTimeout(50);
                if (pressed != 0) break;
            } while (pressed == 0 && !menuShouldExit);

            if (pressed & KEY_B)
                break;
            else if ((pressed & KEY_A) && R_SUCCEEDED(r))
            {
                if((!strncasecmp(displayEntries[selected].ext, "cxi", 3)) && !(!strncasecmp(displayEntries[selected].name, g_twlbgFixedName, 10)))
                {
                    if(R_SUCCEEDED(TwlbgSwitcher_SwitchTwlbg(displayEntries[selected].name)))
                    {
                        if(R_SUCCEEDED(TwlbgSwitcher_WriteNameToFile(displayEntries[selected].name)))
                        {
                            TwlbgSwitcher_UpdateMenu();
                        }
                    }
                    break;
                }                    
            }
            else if (pressed & KEY_DOWN)
                selected++;
            else if (pressed & KEY_UP)
            {
                if(selected > 0)
                    selected--;
            }

            if (selected >= entryCount) selected = 0;

        } while (!menuShouldExit);
    }
}

Result TwlbgSwitcher_PopulateFiles(void)
{
	Result res = 0;
    Handle dir;
    FS_Archive sdmcArchive = 0;
    FS_DirectoryEntry * entries = g_entries;
    Entry * displayEntries = g_displayEntries;

    res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

    res = FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, g_dirPath));

	res = FSDIR_Read(dir, &entryCount, MAX_FILES, entries);

    for (u32 i = 0; i < entryCount; ++i)
    {       
        char filename[FILE_NAME_MAX] = {'\0'};
        Entry *displayEntry = &displayEntries[i];
        utf16_to_utf8((u8 *)filename, entries[i].name, FILE_NAME_MAX - 1); // Convert name from utf16 to utf8
        strcpy((char *)displayEntry->name, filename); // Copy file name
        strcpy((char *)displayEntry->ext, entries[i].shortExt); // Copy ext
    }

	FSDIR_Close(dir);
    FSUSER_CloseArchive(sdmcArchive);

    return res;
}

Result TwlbgSwitcher_SwitchTwlbg(char filename[FILE_NAME_MAX])
{
    Result res = 0;
    FS_Archive sdmcArchive = 0;

    char twlBgPath[512], lastTwlBgPath[512], nextTwlBgPath[512];

    strcpy(twlBgPath, g_dirPath);
    strcat(twlBgPath, "/");
    strcat(twlBgPath, g_twlbgFixedName);
    
    strcpy(lastTwlBgPath, g_dirPath);
    strcat(lastTwlBgPath, "/");
    strlen(g_twlbgName) > 4 ? strcat(lastTwlBgPath, g_twlbgName) : strcat(lastTwlBgPath, g_twlbgFailName);

    sprintf(nextTwlBgPath, "%s/%s", g_dirPath, filename);

    res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

    if(R_SUCCEEDED(res))
            {
                FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, twlBgPath), sdmcArchive, fsMakePath(PATH_ASCII, lastTwlBgPath));
                res = FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, nextTwlBgPath), sdmcArchive, fsMakePath(PATH_ASCII, twlBgPath));
            }

    FSUSER_CloseArchive(sdmcArchive);

    return res;
}

Result TwlbgSwitcher_WriteNameToFile(char filename[FILE_NAME_MAX])
{
    IFile file;
    Result res = 0;

     res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                fsMakePath(PATH_ASCII, g_textFileName), FS_OPEN_CREATE | FS_OPEN_WRITE);
            
            if(R_SUCCEEDED(res))
            {
                u64 total;
                res = IFile_Write(&file, &total, filename, FILE_NAME_MAX, FS_WRITE_FLUSH);
                IFile_Close(&file);

                if(R_SUCCEEDED(res))
                {
                    *g_twlbgName = 0;
                    strcpy(g_twlbgName, filename); 
                }
            }

    return res;
}

Result TwlbgSwitcher_ReadNameFromFile(void)
{
    IFile file;
    Result res = 0;

     res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, g_textFileName), FS_OPEN_READ);
        
        if(R_SUCCEEDED(res))
        {
            u64 total;
            *g_twlbgName = 0;
            IFile_Read(&file, &total, g_twlbgName, sizeof(g_twlbgName));

            IFile_Close(&file);
        }
    
    return res;
}

void TwlbgSwitcher_UpdateMenu(void)
{
    sprintf(g_menuDisplay, "TwlBg: %s", g_twlbgName);

    rosalinaMenu.items[4+isN3DS].title = g_menuDisplay;
}

void TwlbgSwitcher_UpdateStatus(void)
{
    if(R_SUCCEEDED(TwlbgSwitcher_ReadNameFromFile()))
        {
            TwlbgSwitcher_UpdateMenu();
        }
    else
        {
            rosalinaMenu.items[4+isN3DS].title = "TwlBg: unknown";
        }
}
