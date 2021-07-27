#include <stdlib.h>
#include <3ds.h>
#include "fmt.h"
#include "draw.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "menus/quick_switchers.h"

typedef struct Entry {
    char name[FILE_NAME_MAX];       // File name
    char ext[4];        // File extension
} Entry;

typedef struct Switchable {
    char sourceDirPath[32];
    char destinationDirPath[32];
    char persistFileName[64];
    char requiredName[32];
    char unknownName[32];
    char requiredExt[4];
    char menuText[32];
} Switchable;

// Use a global to avoid stack overflow, those structs are quite heavy (advice from Nanquitas plugin loader)
static FS_DirectoryEntry   g_entries[MAX_FILES];
static Entry               g_displayEntries[MAX_FILES];
static Switchable          g_switchables[NO_OF_SWITCHABLES] =
{
    {"/luma/sysmodules/TwlBg", "/luma/sysmodules", "/luma/sysmodules/twlbgName.txt", "TwlBg.cxi", "UnknownTwlBg.cxi", "cxi", "TwlBg"},
    {"/luma/sysmodules/Widescreen", "/_nds/TWiLightMenu/TwlBg", "/luma/sysmodules/widescreenName.txt", "Widescreen.cxi", "UnknownWidescreen.cxi", "cxi", "Widescreen"},
    {"/luma/sysmodules/AgbBg", "/luma/sysmodules", "/luma/sysmodules/agbbgName.txt", "AgbBg.cxi", "UnknownAgbBg.cxi", "cxi", "AgbBg"},
    {"/3ds/open_agb_firm", "/3ds/open_agb_firm", "/3ds/open_agb_firm/configName.txt", "config.ini", "UnknownConfig.ini", "ini", "Open_AGB"}
};

static int g_switchablesIndex = 0;
static char g_persistedName[FILE_NAME_MAX];
static char g_menuDisplay[NO_OF_SWITCHABLES][266];

u32 entryCount = 0;

Menu quickSwitchersMenu = {
    "Quick-Switchers menu",
    {
        { g_switchables[0].menuText, METHOD, .method = &QuickSwitchers_TwlBg},
        { g_switchables[1].menuText, METHOD, .method = &QuickSwitchers_Widescreen},
        { g_switchables[2].menuText, METHOD, .method = &QuickSwitchers_AgbBg},
        { g_switchables[3].menuText, METHOD, .method = &QuickSwitchers_OpenAgb},
        { "Revert TWL widescreen", METHOD, .method = &QuickSwitchers_RevertWidescreen},
        {},
    }
};

void QuickSwitchers_TwlBg(void)
{
    g_switchablesIndex = 0;
    QuickSwitchers_DisplayFiles();
}

void QuickSwitchers_Widescreen(void)
{
    g_switchablesIndex = 1;
    QuickSwitchers_DisplayFiles();
}

void QuickSwitchers_AgbBg(void)
{
    g_switchablesIndex = 2;
    QuickSwitchers_DisplayFiles();
}

void QuickSwitchers_OpenAgb(void)
{
    g_switchablesIndex = 3;
    QuickSwitchers_DisplayFiles();
}

void QuickSwitchers_DisplayFiles(void)
{   
	Result r = QuickSwitchers_PopulateFiles();
    QuickSwitchers_UpdateStatus();

	Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

	if (entryCount == 0)
    {
        do
        {
            Draw_Lock();
            Draw_DrawFormattedString(10, 10, COLOR_TITLE, "%s Quick-Switcher - by Nutez", g_switchables[g_switchablesIndex].menuText);

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
                Draw_DrawFormattedString(10, 10, COLOR_TITLE, "%s Quick-Switcher - by Nutez", g_switchables[g_switchablesIndex].menuText);

                for (u32 i = 0; i < entryCount; i++)
                {
                    Draw_DrawString(30, 30 + i * SPACING_Y, ((!strncasecmp(displayEntries[i].ext, g_switchables[g_switchablesIndex].requiredExt, 3)) 
                            && !(!strncasecmp(displayEntries[i].name, g_switchables[g_switchablesIndex].requiredName, 10)))
                                ? COLOR_WHITE : COLOR_RED, displayEntries[i].name);
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
                if((!strncasecmp(displayEntries[selected].ext, g_switchables[g_switchablesIndex].requiredExt, 3)) && !(!strncasecmp(displayEntries[selected].name, g_switchables[g_switchablesIndex].requiredName, 10)))
                {
                    if(R_SUCCEEDED(QuickSwitchers_SwitchFile(displayEntries[selected].name)))
                    {
                        if(R_SUCCEEDED(QuickSwitchers_WriteNameToFile(displayEntries[selected].name)))
                        {
                            QuickSwitchers_UpdateMenu();
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

Result QuickSwitchers_PopulateFiles(void)
{
	Result res = 0;
    Handle dir;
    FS_Archive sdmcArchive = 0;
    FS_DirectoryEntry * entries = g_entries;
    Entry * displayEntries = g_displayEntries;

    res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

    res = FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, g_switchables[g_switchablesIndex].sourceDirPath));

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

Result QuickSwitchers_SwitchFile(char filename[FILE_NAME_MAX])
{
    Result res = 0;
    FS_Archive sdmcArchive = 0;

    char activeFilePath[512]; // path & name required for file to be usable for external programme
    char switchOutPath[512]; // path & name to place previously "active" file
    char switchInPath[512]; // path & name of file select to become "active" file

    strcpy(activeFilePath, g_switchables[g_switchablesIndex].destinationDirPath);
    strcat(activeFilePath, "/");
    strcat(activeFilePath, g_switchables[g_switchablesIndex].requiredName); 
    
    strcpy(switchOutPath, g_switchables[g_switchablesIndex].sourceDirPath);
    strcat(switchOutPath, "/");
    strlen(g_persistedName) > 4 ? strcat(switchOutPath, g_persistedName) : strcat(switchOutPath, g_switchables[g_switchablesIndex].unknownName);

    sprintf(switchInPath, "%s/%s", g_switchables[g_switchablesIndex].sourceDirPath, filename);

    res = FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

    if(R_SUCCEEDED(res))
    {
        FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, activeFilePath), sdmcArchive, fsMakePath(PATH_ASCII, switchOutPath));
        res = FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, switchInPath), sdmcArchive, fsMakePath(PATH_ASCII, activeFilePath)); // renames chosen file to the required name for external programme
    }

    FSUSER_CloseArchive(sdmcArchive);

    return res;
}

Result QuickSwitchers_WriteNameToFile(char filename[FILE_NAME_MAX])
{
    IFile file;
    Result res = 0;
    *g_persistedName = 0;

     res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
                fsMakePath(PATH_ASCII, g_switchables[g_switchablesIndex].persistFileName), FS_OPEN_CREATE | FS_OPEN_WRITE);
            
            if(R_SUCCEEDED(res))
            {
                u64 total;
                res = IFile_Write(&file, &total, filename, FILE_NAME_MAX, FS_WRITE_FLUSH);
                IFile_Close(&file);

                if(R_SUCCEEDED(res))
                {         
                    strcpy(g_persistedName, filename); 
                }
            }

    return res;
}

Result QuickSwitchers_ReadNameFromFile(void)
{
    IFile file;
    Result res = 0;
    *g_persistedName = 0;

     res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, g_switchables[g_switchablesIndex].persistFileName), FS_OPEN_READ);
        
        if(R_SUCCEEDED(res))
        {
            u64 total;
            IFile_Read(&file, &total, g_persistedName, sizeof(g_persistedName));

            IFile_Close(&file);
        }
    
    return res;
}

void QuickSwitchers_UpdateMenu(void)
{
    sprintf(g_menuDisplay[g_switchablesIndex], "%s: %s", g_switchables[g_switchablesIndex].menuText, g_persistedName);
    quickSwitchersMenu.items[g_switchablesIndex].title = g_menuDisplay[g_switchablesIndex];
}

void QuickSwitchers_UpdateStatus(void)
{
    if(R_SUCCEEDED(QuickSwitchers_ReadNameFromFile()))
    {
        QuickSwitchers_UpdateMenu();
    }
    else
    {
        sprintf(g_menuDisplay[g_switchablesIndex], "%s: %s", g_switchables[g_switchablesIndex].menuText, "unknown");
        quickSwitchersMenu.items[g_switchablesIndex].title = g_menuDisplay[g_switchablesIndex];
    }
}

void QuickSwitchers_UpdateStatuses(void)
{
    g_switchablesIndex = 0;

    for (u32 i = 0; i < NO_OF_SWITCHABLES; ++i)
    {
        QuickSwitchers_UpdateStatus();
        ++g_switchablesIndex;
    }
}

void QuickSwitchers_RevertWidescreen(void)
{
    FS_Archive sdmcArchive = 0;

    const char twlbgPath[32] = "/luma/sysmodules/TwlBg.cxi";
    const char twlbgBakPath[42] = "/_nds/TWiLightMenu/TwlBg/TwlBg.cxi.bak";
    const char widescreenPath[42] = "/_nds/TWiLightMenu/TwlBg/Widescreen.cxi";

    if(R_SUCCEEDED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        MenuItem *item = &quickSwitchersMenu.items[4];

        if(R_SUCCEEDED(FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, twlbgPath), sdmcArchive, fsMakePath(PATH_ASCII, widescreenPath))))
        {
            if(R_SUCCEEDED(FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, twlbgBakPath), sdmcArchive, fsMakePath(PATH_ASCII, twlbgPath)))) // renames chosen file to the required name for external programme
            {
                item->title = "Revert TWL widescreen: [Succeeded]";
            }
            else 
            {
                FSUSER_RenameFile(sdmcArchive, fsMakePath(PATH_ASCII, widescreenPath), sdmcArchive, fsMakePath(PATH_ASCII, twlbgPath));
                item->title = "Revert TWL widescreen: [Unneeded]";
            }
        }
        else
        {
            item->title = "Revert TWL widescreen: [Unneeded]";
        }

        FSUSER_CloseArchive(sdmcArchive);
    }
}
