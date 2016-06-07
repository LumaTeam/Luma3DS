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
#include "exceptions.h"
#include "draw.h"
#include "screeninit.h"
#include "buttons.h"
#include "../build/injector.h"

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
        a9lhMode,
        updatedSys,
        needConfig,
        newConfig,
        emuHeader,
        chronoStarted = 0;

    //Detect the console being used
    console = PDN_MPCORE_CFG == 7;

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    const char configPath[] = "/luma/config.bin";

    //Attempt to read the configuration file
    needConfig = fileRead(&config, configPath) ? 1 : 2;

    //Determine if this is a firmlaunch boot
    if(*(vu8 *)0x23F00005)
    {
        if(needConfig == 2) mcuReboot();

        bootType = 1;

        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = *(vu8 *)0x23F00009 == '3' ? 3 : *(vu8 *)0x23F00005 - '0';

        nandType = BOOTCONFIG(0, 3);
        firmSource = BOOTCONFIG(2, 1);
        a9lhMode = BOOTCONFIG(3, 1);
        updatedSys = a9lhMode && CONFIG(1);
    }
    else
    {
        //Get pressed buttons
        u32 pressed = HID_PAD;

        //If no configuration file exists or SELECT is held, load configuration menu
        if(needConfig == 2 || ((pressed & BUTTON_SELECT) && !(pressed & BUTTON_L1)))
        {
            configureCFW(configPath);

            //Zero the last booted FIRM flag
            CFG_BOOTENV = 0;

            chronoStarted = 1;
            chrono(0);
            chrono(2);

            //Update pressed buttons
            pressed = HID_PAD;
        }

        if(DEVMODE) 
        { 
            detectAndProcessExceptionDumps();
            installArm9Handlers();
        }

        bootType = 0;
        firmType = 0;

        //Determine if booting with A9LH
        u32 a9lhBoot = !PDN_SPI_CNT;

        //Determine if A9LH is installed and the user has an updated sysNAND
        if(a9lhBoot || CONFIG(2))
        {
            a9lhMode = 1;
            updatedSys = CONFIG(1);
        }
        else
        {
            a9lhMode = 0;
            updatedSys = 0;
        }

        newConfig = a9lhMode << 3;

        if(a9lhBoot)
        {
            //Retrieve the last booted FIRM
            u32 previousFirm = CFG_BOOTENV;

            //If it's a MCU reboot, try to force boot options
            if(previousFirm)
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

                /* Else, force the last used boot options unless a button is pressed
                    or the no-forcing flag is set */
                else if(!pressed && !BOOTCONFIG(4, 1))
                {
                    nandType = BOOTCONFIG(0, 3);
                    firmSource = BOOTCONFIG(2, 1);
                    needConfig = 0;
                }
            }

            //If the SAFE MODE combo is held, force a sysNAND boot
            else if(pressed == SAFE_MODE)
            {
                a9lhMode = 2;
                nandType = 0;
                firmSource = 0;
                needConfig = 0;
            }
        }

        //Boot options aren't being forced
        if(needConfig)
        {
            /* If L and R/A/Select or one of the single payload buttons are pressed,
               chainload an external payload */
            if(DEVMODE || (pressed & SINGLE_PAYLOAD_BUTTONS) || ((pressed & BUTTON_L1) && (pressed & L_PAYLOAD_BUTTONS)))
                loadPayload(pressed);

            //If screens are inited or the corresponding option is set, load splash screen
            if((PDN_GPU_CNT != 1 || CONFIG(7)) && loadSplash())
            {
                chronoStarted = 2;
                chrono(0);
            }

            //If R is pressed, boot the non-updated NAND with the FIRM of the opposite one
            if(pressed & BUTTON_R1)
            {
                nandType = updatedSys;
                firmSource = !nandType;
            }

            /* Else, boot the NAND the user set to autoboot or the opposite one, depending on L,
               with their own FIRM */
            else
            {
                nandType = CONFIG(0) != !(pressed & BUTTON_L1);
                firmSource = nandType;
            }

            /* If we're booting emuNAND the second emuNAND is set as default and B isn't pressed,
               or vice-versa, boot the second emuNAND */
            if(nandType && (CONFIG(3) == !(pressed & BUTTON_B))) nandType = 2;
        }
    }

    //If we need to boot emuNAND, make sure it exists
    if(nandType)
    {
        locateEmuNAND(&emuOffset, &emuHeader, &nandType);
        if(!nandType) firmSource = 0;
    }

    //Same if we're using emuNAND as the FIRM source
    else if(firmSource)
        locateEmuNAND(&emuOffset, &emuHeader, &firmSource);

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
    
    switch(firmType)
    {
        case 0:
            patchNativeFirm(nandType, emuHeader, a9lhMode);
            break;
        case 3:
            patchSafeFirm();
            break;
        default:
            patchLegacyFirm(firmType);
            break;
    }

    if(chronoStarted)
    {
        if(chronoStarted == 2) chrono(3);
        stopChrono();
    }

    launchFirm(firmType, bootType);
}

static inline void loadFirm(u32 firmType, u32 externalFirm)
{
    section = firm->section;

    u32 externalFirmLoaded = externalFirm &&
                             fileRead(firm, "/luma/firmware.bin") &&
                             (((u32)section[2].address >> 8) & 0xFF) == (console ? 0x60 : 0x68);

    /* If the conditions to load the external FIRM aren't met, or reading fails, or the FIRM
       doesn't match the console, load FIRM from CTRNAND */
    if(!externalFirmLoaded)
    {
        const char *firmFolders[4][2] = {{ "00000002", "20000002" },
                                         { "00000102", "20000102" },
                                         { "00000202", "20000202" },
                                         { "00000003", "20000003" }};

        firmRead(firm, firmFolders[firmType][console]);
        decryptExeFs((u8 *)firm);
    }
}

static inline void patchNativeFirm(u32 nandType, u32 emuHeader, u32 a9lhMode)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset,
       *arm11Section1 = (u8 *)firm + section[1].offset;

    u32 nativeFirmType;

    if(console)
    {
        //Determine the NATIVE_FIRM version
        switch(arm9Section[0x53])
        {
            case 0xFF:
                nativeFirmType = 0;
                break;
            case '1':
                nativeFirmType = 2;
                break;
            default:
                nativeFirmType = 1;
                break;
        }

        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, nativeFirmType);
        firm->arm9Entry = (u8 *)0x801B01C;
    }
    else
    {
        //Determine if we're booting the 9.0 FIRM
        u8 firm90Hash[0x10] = {0x27, 0x2D, 0xFE, 0xEB, 0xAF, 0x3F, 0x6B, 0x3B, 0xF5, 0xDE, 0x4C, 0x41, 0xDE, 0x95, 0x27, 0x6A};
        nativeFirmType = memcmp(section[2].hash, firm90Hash, 0x10) != 0;
    }

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9(arm9Section + 0x15000, section[2].size - 0x15000, &process9Size, &process9MemAddr);

    //Apply signature patches
    patchSignatureChecks(process9Offset, process9Size);

    //Apply emuNAND patches
    if(nandType)
    {
        u32 branchAdditive = (u32)firm + section[2].offset - (u32)section[2].address;
        patchEmuNAND(arm9Section, section[2].size, process9Offset, process9Size, emuOffset, emuHeader, branchAdditive);
    }

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    else if(a9lhMode) patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches, not on 9.0 FIRM as it breaks firmlaunchhax
    if(nativeFirmType || a9lhMode == 2) patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    if(nativeFirmType == 1)
    {
        //Apply anti-anti-DG patches for >= 11.0 firmwares
        patchTitleInstallMinVersionCheck(process9Offset, process9Size);

        //Does nothing if svcBackdoor is still there
        reimplementSvcBackdoor(arm11Section1, section[1].size);
    }

    if(DEVMODE)
    {
        //Apply UNITINFO patch
        if(DEVMODE == 2) patchUnitInfoValueSet(arm9Section, section[2].size);
        
        //Install arm11 exception handlers
        u32 stackAddress, codeSetOffset;
        u32 *exceptionsPage = getInfoForArm11ExceptionHandlers(arm11Section1, section[1].size, &stackAddress, &codeSetOffset);
        installArm11Handlers(exceptionsPage, stackAddress, codeSetOffset);
        
        //Kernel9/Process9 debugging
        patchExceptionHandlersInstall(arm9Section, section[2].size);
        patchSvcBreak9(arm9Section, section[2].size, (u32)(section[2].address));
        
        //Stub svcBreak11 with "bkpt 65535"
        patchSvcBreak11(arm11Section1, section[1].size);
        
        //Make FCRAM (and VRAM as a side effect) globally executable from arm11 kernel
        patchKernelFCRAMAndVRAMMappingPermissions(arm11Section1, section[1].size);
    }
}

static inline void patchLegacyFirm(u32 firmType)
{
    u8 *arm9Section = (u8 *)firm + section[3].offset;
    
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(console)
    {
        arm9Loader(arm9Section, 0);
        firm->arm9Entry = (u8 *)0x801301C;
    }
    
    if(DEVMODE)
    {
        //Kernel9/Process9 debugging
        patchExceptionHandlersInstall(arm9Section, section[3].size);
        patchSvcBreak9(arm9Section, section[3].size, (u32)(section[3].address));
    }

    applyLegacyFirmPatches((u8 *)firm, firmType, console);
}

static inline void patchSafeFirm(void)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    if(console)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, 0);
        firm->arm9Entry = (u8 *)0x801B01C;

        patchFirmWrites(arm9Section, section[2].size);
    }
    
    if(DEVMODE)
    {
        //Kernel9/Process9 debugging
        patchExceptionHandlersInstall(arm9Section, section[2].size);
        patchSvcBreak9(arm9Section, section[2].size, (u32)(section[2].address));
    }
    
    else patchFirmWriteSafe(arm9Section, section[2].size);
}

static inline void copySection0AndInjectLoader(void)
{
    u8 *arm11Section0 = (u8 *)firm + section[0].offset;

    u32 loaderSize;
    u32 loaderOffset = getLoader(arm11Section0, &loaderSize);

    memcpy(section[0].address, arm11Section0, loaderOffset);
    memcpy(section[0].address + loaderOffset, injector, injector_size);
    memcpy(section[0].address + loaderOffset + injector_size, arm11Section0 + loaderOffset + loaderSize, section[0].size - (loaderOffset + loaderSize));
}

static inline void launchFirm(u32 firmType, u32 bootType)
{
    //If we're booting NATIVE_FIRM, section0 needs to be copied separately to inject 3ds_injector
    u32 sectionNum;
    if(!firmType)
    {
        copySection0AndInjectLoader();
        sectionNum = 1;
    }
    else sectionNum = 0;

    //Copy FIRM sections to respective memory locations
    for(; sectionNum < 4 && section[sectionNum].size; sectionNum++)
        memcpy(section[sectionNum].address, (u8 *)firm + section[sectionNum].offset, section[sectionNum].size);

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