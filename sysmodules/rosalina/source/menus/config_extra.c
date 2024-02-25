#include <3ds.h>
#include "fmt.h"
#include "draw.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "menus/config_extra.h"

config_extra configExtra = { .suppressLeds = false, .cutSlotPower = false, .homeToRosalina = false, .toggleBtmLCD = true };
bool configExtraSaved = false;

static const char menuText[5][32] = {
    "Automatically suppress LEDs",
    "Cut power to TWL Flashcards",
    "Home button opens Rosalina",
    "st+sel toggle bottom LCD",
    "Save config. Changes saved"
};

static char menuDisplay[5][64];

Menu configExtraMenu = {
    "Extra config menu",
    {
        { menuText[0], METHOD, .method = &ConfigExtra_SetSuppressLeds},
        { menuText[1], METHOD, .method = &ConfigExtra_SetCutSlotPower},
        { menuText[2], METHOD, .method = &ConfigExtra_SetHomeToRosalina},
        { menuText[3], METHOD, .method = &ConfigExtra_SetToggleBtmLCD},
        { menuText[4], METHOD, .method = &ConfigExtra_WriteConfigExtra},
        {},
    }
};

void ConfigExtra_SetSuppressLeds(void) 
{
    configExtra.suppressLeds = !configExtra.suppressLeds;
    ConfigExtra_UpdateMenuItem(0, configExtra.suppressLeds);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(4, configExtraSaved);
}

void ConfigExtra_SetCutSlotPower(void) 
{
    configExtra.cutSlotPower = !configExtra.cutSlotPower;
    ConfigExtra_UpdateMenuItem(1, configExtra.cutSlotPower);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(4, configExtraSaved);
}

void ConfigExtra_SetHomeToRosalina(void) 
{
    configExtra.homeToRosalina = !configExtra.homeToRosalina;
    ConfigExtra_UpdateMenuItem(2, configExtra.homeToRosalina);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(4, configExtraSaved);
}

void ConfigExtra_SetToggleBtmLCD(void)
{
    configExtra.toggleBtmLCD = !configExtra.toggleBtmLCD;
    ConfigExtra_UpdateMenuItem(3, configExtra.toggleBtmLCD);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(4, configExtraSaved);
}

void ConfigExtra_UpdateMenuItem(int menuIndex, bool value)
{
    sprintf(menuDisplay[menuIndex], "%s: %s", menuText[menuIndex], value ? "[true]" : "[false]");
    configExtraMenu.items[menuIndex].title = menuDisplay[menuIndex];
}

void ConfigExtra_UpdateAllMenuItems(void)
{
    ConfigExtra_UpdateMenuItem(0, configExtra.suppressLeds);
    ConfigExtra_UpdateMenuItem(1, configExtra.cutSlotPower);
    ConfigExtra_UpdateMenuItem(2, configExtra.homeToRosalina);
    ConfigExtra_UpdateMenuItem(3, configExtra.toggleBtmLCD);
    ConfigExtra_UpdateMenuItem(4, configExtraSaved);
}

void ConfigExtra_ReadConfigExtra(void)
{
    IFile file;
    Result res = 0;

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, "/luma/configExtra.bin"), FS_OPEN_READ);
        
    if(R_SUCCEEDED(res))
    {
        u64 total;
        res = IFile_Read(&file, &total, &configExtra, sizeof(configExtra));
        IFile_Close(&file);
        if(R_SUCCEEDED(res)) 
        {
            configExtraSaved = true;
        }
    }
}

void ConfigExtra_WriteConfigExtra(void)
{
    IFile file;
    Result res = 0;

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, "/luma/configExtra.bin"), FS_OPEN_CREATE | FS_OPEN_WRITE);
        
    if(R_SUCCEEDED(res))
    {
        u64 total;
        res = IFile_Write(&file, &total, &configExtra, sizeof(configExtra), 0);
        IFile_Close(&file);

        if(R_SUCCEEDED(res)) 
        {
            configExtraSaved = true;
            ConfigExtra_UpdateMenuItem(4, configExtraSaved);
        }
    }
}
