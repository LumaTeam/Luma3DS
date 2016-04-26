/*
*   firm.c
*/

#include "firm.h"
#include "config.h"
#include "utils.h"
#include "fs.h"
#include "patches.h"
#include "memory.h"
#include "emunand.h"
#include "crypto.h"
#include "draw.h"
#include "screeninit.h"
#include "loader.h"
#include "exceptions.h"
#include "buttons.h"
#include "../build/patches.h"

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;

u32 config,
    console,
    firmSource,
    emuOffset;

void main(void)
{
    u32 bootType,
        firmType,
        nandType,
        a9lhInstalled,
        updatedSys,
        needConfig,
        newConfig,
        emuHeader;

    //Detect the console being used
    console = (PDN_MPCORE_CFG == 1) ? 0 : 1;

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    //Attempt to read the configuration file
    const char configPath[] = "/luma/config.bin";
    if(fileRead(&config, configPath, 4)) needConfig = 1;
    else
    {
        config = 0;
        needConfig = 2;
    }

    //Determine if this is a firmlaunch boot
    if(*(vu8 *)0x23F00005)
    {
        if(needConfig == 2) mcuReboot();

        bootType = 1;

        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = *(vu8 *)0x23F00005 - '0';

        nandType = BOOTCONFIG(0, 3);
        firmSource = BOOTCONFIG(2, 1);
        a9lhInstalled = BOOTCONFIG(3, 1);
        updatedSys = (a9lhInstalled && CONFIG(1)) ? 1 : 0;
    }
    else
    {
        //Only when "Enable developer features" is set
        if(CONFIG(5))
        {
            detectAndProcessExceptionDumps();
            installArm9Handlers();
        }

        bootType = 0;
        firmType = 0;

        //Determine if booting with A9LH
        u32 a9lhBoot = !PDN_SPI_CNT ? 1 : 0;

        //Retrieve the last booted FIRM
        u32 previousFirm = CFG_BOOTENV;

        //Get pressed buttons
        u32 pressed = HID_PAD;

        //Determine if A9LH is installed and the user has an updated sysNAND
        if(a9lhBoot || CONFIG(2))
        {
            if(pressed == SAFE_MODE)
                error("Using Safe Mode would brick you, or remove A9LH!");

            a9lhInstalled = 1;

            //Check setting for > 9.2 sysNAND
            updatedSys = CONFIG(1);
        }
        else
        {
            a9lhInstalled = 0;
            updatedSys = 0;
        }

        newConfig = a9lhInstalled << 3;

        /* If booting with A9LH, it's a MCU reboot and a previous configuration exists,
           try to force boot options */
        if(a9lhBoot && previousFirm && needConfig == 1)
        {
            //Always force a sysNAND boot when quitting AGB_FIRM
            if(previousFirm == 7)
            {
                nandType = 0;
                firmSource = updatedSys ? 0 : BOOTCONFIG(2, 1);
                needConfig = 0;

                //Flag to prevent multiple boot options-forcing
                newConfig |= 1 << 4;
            }
            /* Else, force the last used boot options unless a payload button or A/L/R are pressed
               or the no-forcing flag is set */
            else if(!(pressed & OVERRIDE_BUTTONS) && !BOOTCONFIG(4, 1))
            {
                nandType = BOOTCONFIG(0, 3);
                firmSource = BOOTCONFIG(2, 1);
                needConfig = 0;
            }
        }

        //Boot options aren't being forced
        if(needConfig)
        {
            /* If L and R/Select or one of the single payload buttons are pressed and, if not using A9LH,
               the Safe Mode combo is not pressed, chainload an external payload */
            if(((pressed & SINGLE_PAYLOAD_BUTTONS) || ((pressed & BUTTON_L1) && (pressed & L_PAYLOAD_BUTTONS)))
               && pressed != SAFE_MODE)
                loadPayload();

            //If no configuration file exists or SELECT is held, load configuration menu
            if(needConfig == 2 || (pressed & BUTTON_SELECT))
                configureCFW(configPath);

            //If screens are inited or the corresponding option is set, load splash screen
            if(PDN_GPU_CNT != 1 || CONFIG(8)) loadSplash();

            u32 autoBootSys = CONFIG(0);

            //Determine if we need to boot an emuNAND or sysNAND
            nandType = (pressed & BUTTON_L1) ? autoBootSys : ((pressed & BUTTON_R1) ? updatedSys : !autoBootSys);

            /* If we're booting emuNAND the second emuNAND is set as default and B isn't pressed,
               or vice-versa, boot the second emuNAND */
            if(nandType && ((!(pressed & BUTTON_B)) == CONFIG(3))) nandType++;

            //Determine the NAND we should take the FIRM from
            firmSource = (pressed & BUTTON_R1) ? !nandType : (nandType != 0);
        }
    }

    //If we need to boot emuNAND, make sure it exists
    if(nandType)
    {
        getEmunandSect(&emuOffset, &emuHeader, &nandType);
        if(!nandType) firmSource = 0;
    }

    //Same if we're using emuNAND as the FIRM source
    else if(firmSource)
        getEmunandSect(&emuOffset, &emuHeader, &firmSource);

    if(!bootType)
    {
        newConfig |= nandType | (firmSource << 2);

        /* If the boot configuration is different from previously, overwrite it.
           Just the no-forcing flag being set is not enough */
        if((newConfig & 0x2F) != (config & 0x3F))
        {
            //Preserve user settings (last 26 bits)
            newConfig |= config & 0xFFFFFFC0;

            fileWrite(&newConfig, configPath, 4);
        }
    }

    loadFirm(firmType, !firmType && updatedSys == !firmSource);

    if(!firmType) patchNativeFirm(nandType, emuHeader, a9lhInstalled);
    else patchTwlAgbFirm(firmType);

    launchFirm(bootType);
}

static inline void loadFirm(u32 firmType, u32 externalFirm)
{
    section = firm->section;

    /* If the conditions to load the external FIRM aren't met, or reading fails, or the FIRM
       doesn't match the console, load it from CTRNAND */
    if(!externalFirm || !fileRead(firm, "/luma/firmware.bin", 0) ||
       (((u32)section[2].address >> 8) & 0xFF) != (console ? 0x60 : 0x68))
    {
        const char *firmFolders[3][2] = {{ "00000002", "20000002" },
                                         { "00000102", "20000102" },
                                         { "00000202", "20000202" }};

        firmRead(firm, firmFolders[firmType][console]);
        decryptExeFs((u8 *)firm);
    }
}

static inline void patchNativeFirm(u32 nandType, u32 emuHeader, u32 a9lhInstalled)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    u32 nativeFirmType;

    if(console)
    {
        //Determine if we're booting the 9.0 FIRM
        nativeFirmType = (arm9Section[0x51] == 0xFF) ? 0 : 1;

        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, nativeFirmType);
        firm->arm9Entry = (u8 *)0x801B01C;
    }
    else
    {
        //Determine if we're booting the 9.0 FIRM
        u8 firm90Hash[0x10] = {0x27, 0x2D, 0xFE, 0xEB, 0xAF, 0x3F, 0x6B, 0x3B, 0xF5, 0xDE, 0x4C, 0x41, 0xDE, 0x95, 0x27, 0x6A};
        nativeFirmType = (memcmp(section[2].hash, firm90Hash, 0x10) == 0) ? 0 : 1;
    }

    if(nativeFirmType || nandType)
    {
        //Find the Process9 NCCH location
        u8 *proc9Offset = getProc9(arm9Section, section[2].size);

        //Apply emuNAND patches
        if(nandType) patchEmuNAND(arm9Section, proc9Offset, emuHeader);

        //Apply FIRM reboot patches, not on 9.0 FIRM as it breaks firmlaunchhax
        if(nativeFirmType) patchReboots(arm9Section, proc9Offset);
    }

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    if(a9lhInstalled && !nandType)
    {
        u16 *writeOffset = getFirmWrite(arm9Section, section[2].size);
        *writeOffset = writeBlock[0];
        *(writeOffset + 1) = writeBlock[1];
    }

    //Apply signature checks patches
    u32 sigOffset,
        sigOffset2;

    getSigChecks(arm9Section, section[2].size, &sigOffset, &sigOffset2);
    *(u16 *)sigOffset = sigPatch[0];
    *(u16 *)sigOffset2 = sigPatch[0];
    *((u16 *)sigOffset2 + 1) = sigPatch[1];

    if(CONFIG(5))
    {
        //Apply UNITINFO patch
        u8 *unitInfoOffset = getUnitInfoValueSet(arm9Section, section[2].size);
        *unitInfoOffset = unitInfoPatch;
    }

    //Replace the FIRM loader with the injector
    injectLoader();
}

static inline void patchEmuNAND(u8 *arm9Section, u8 *proc9Offset, u32 emuHeader)
{
    //Copy emuNAND code
    void *emuCodeOffset = getEmuCode(proc9Offset);
    memcpy(emuCodeOffset, emunand, emunand_size);

    //Add the data of the found emuNAND
    u32 *pos_offset = (u32 *)memsearch(emuCodeOffset, "NAND", emunand_size, 4);
    u32 *pos_header = (u32 *)memsearch(emuCodeOffset, "NCSD", emunand_size, 4);
    *pos_offset = emuOffset;
    *pos_header = emuHeader;

    //Find and add the SDMMC struct
    u32 *pos_sdmmc = (u32 *)memsearch(emuCodeOffset, "SDMC", emunand_size, 4);
    *pos_sdmmc = getSDMMC(arm9Section, section[2].size);

    //Calculate offset for the hooks
    u32 branchOffset = (u32)emuCodeOffset - (u32)firm -
                       section[2].offset + (u32)section[2].address;

    //Add emuNAND hooks
    u32 emuRead,
        emuWrite;

    getEmuRW(arm9Section, section[2].size, &emuRead, &emuWrite);
    *(u16 *)emuRead = nandRedir[0];
    *((u16 *)emuRead + 1) = nandRedir[1];
    *((u32 *)emuRead + 1) = branchOffset;
    *(u16 *)emuWrite = nandRedir[0];
    *((u16 *)emuWrite + 1) = nandRedir[1];
    *((u32 *)emuWrite + 1) = branchOffset;

    //Set MPU for emu code region
    u32 *mpuOffset = getMPU(arm9Section, section[2].size);
    *mpuOffset = mpuPatch[0];
    *(mpuOffset + 6) = mpuPatch[1];
    *(mpuOffset + 9) = mpuPatch[2];
}

static inline void patchReboots(u8 *arm9Section, u8 *proc9Offset)
{
    //Calculate offset for the firmlaunch code
    void *rebootOffset = getReboot(arm9Section, section[2].size);

    //Calculate offset for the fOpen function
    u32 fOpenOffset = getfOpen(proc9Offset, rebootOffset);

    //Copy firmlaunch code
    memcpy(rebootOffset, reboot, reboot_size);

    //Put the fOpen offset in the right location
    u32 *pos_fopen = (u32 *)memsearch(rebootOffset, "OPEN", reboot_size, 4);
    *pos_fopen = fOpenOffset;
}

static inline void injectLoader(void)
{
    u32 loaderSize;

    void *loaderOffset = getLoader((u8 *)firm + section[0].offset, section[0].size, &loaderSize);

    //Check that the injector CXI isn't larger than the original
    if((u32)injector_size <= loaderSize)
    {
        memcpy(loaderOffset, injector, injector_size);

        //Patch content size and ExeFS size to match the repaced loader's ones
        *((u32 *)loaderOffset + 0x41) = loaderSize / 0x200;
        *((u32 *)loaderOffset + 0x69) = loaderSize / 0x200 - 5;
    }
}

static inline void patchTwlAgbFirm(u32 firmType)
{
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(console)
    {
        arm9Loader((u8 *)firm + section[3].offset, 0);
        firm->arm9Entry = (u8 *)0x801301C;
    }

    const patchData twlPatches[] = {
        {{0x1650C0, 0x165D64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0x173A0E, 0x17474A}, { .type1 = 0x2001 }, 1},
        {{0x174802, 0x17553E}, { .type1 = 0x2000 }, 2},
        {{0x174964, 0x1756A0}, { .type1 = 0x2000 }, 2},
        {{0x174D52, 0x175A8E}, { .type1 = 0x2001 }, 2},
        {{0x174D5E, 0x175A9A}, { .type1 = 0x2001 }, 2},
        {{0x174D6A, 0x175AA6}, { .type1 = 0x2001 }, 2},
        {{0x174E56, 0x175B92}, { .type1 = 0x2001 }, 1},
        {{0x174E58, 0x175B94}, { .type1 = 0x4770 }, 1}
    },
    agbPatches[] = {
        {{0x9D2A8, 0x9DF64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0xD7A12, 0xD8B8A}, { .type1 = 0xEF26 }, 1}
    };

    /* Calculate the amount of patches to apply. Only count the boot screen patch for AGB_FIRM
       if the matching option was enabled (keep it as last) */
    u32 numPatches = firmType == 1 ? (sizeof(twlPatches) / sizeof(patchData)) :
                                     (sizeof(agbPatches) / sizeof(patchData) - !CONFIG(7));
    const patchData *patches = firmType == 1 ? twlPatches : agbPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
                memcpy((u8 *)firm + patches[i].offset[console], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
                break;
            case 2:
                *(u16 *)((u8 *)firm + patches[i].offset[console] + 2) = 0;
            case 1:
                *(u16 *)((u8 *)firm + patches[i].offset[console]) = patches[i].patch.type1;
                break;
        }
    }
}

static inline void launchFirm(u32 bootType)
{
    //Copy FIRM sections to respective memory locations
    for(u32 i = 0; i < 4 && section[i].size; i++)
        memcpy(section[i].address, (u8 *)firm + section[i].offset, section[i].size);

    //Determine the ARM11 entry to use
    vu32 *arm11;
    if(bootType) arm11 = (u32 *)0x1FFFFFFC;
    else
    {
        deinitScreens();
        arm11 = (u32 *)0x1FFFFFF8;
    }

    //Set ARM11 kernel entrypoint
    *arm11 = (u32)firm->arm11Entry;

    //Final jump to ARM9 kernel
    ((void (*)())firm->arm9Entry)();
}