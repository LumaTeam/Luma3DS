/*
*   firm.c
*/

#include "firm.h"
#include "config.h"
#include "utils.h"
#include "fs.h"
#include "patches.h"
#include "memory.h"
#include "cache.h"
#include "emunand.h"
#include "crypto.h"
#include "exceptions.h"
#include "draw.h"
#include "screen.h"
#include "buttons.h"
#include "../build/injector.h"

static firmHeader *const firm = (firmHeader *)0x24000000;
static const firmSectionHeader *section;

u32 config,
    emuOffset;

bool isN3DS;

FirmwareSource firmSource;

void main(void)
{
    bool isFirmlaunch,
         updatedSys;

    u32 newConfig,
        emuHeader,
        nbChronoStarted = 0;

    FirmwareType firmType;
    FirmwareSource nandType;
    ConfigurationStatus needConfig;
    A9LHMode a9lhMode;

    //Detect the console being used
    isN3DS = PDN_MPCORE_CFG == 7;

    //Mount filesystems. CTRNAND will be mounted only if/when needed
    mountFs();

    const char configPath[] = "/luma/config.bin";

    //Attempt to read the configuration file
    needConfig = fileRead(&config, configPath) ? MODIFY_CONFIGURATION : CREATE_CONFIGURATION;

    if(DEVMODE) 
    { 
            detectAndProcessExceptionDumps();
            installArm9Handlers();
    }
        
    //Determine if this is a firmlaunch boot
    if(*(vu8 *)0x23F00005)
    {
        if(needConfig == CREATE_CONFIGURATION) mcuReboot();

        isFirmlaunch = true;

        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = *(vu8 *)0x23F00009 == '3' ? SAFE_FIRM : (FirmwareType)(*(vu8 *)0x23F00005 - '0');

        nandType = (FirmwareSource)BOOTCONFIG(0, 3);
        firmSource = (FirmwareSource)BOOTCONFIG(2, 1);
        a9lhMode = (A9LHMode)BOOTCONFIG(3, 1);
        updatedSys = a9lhMode != NO_A9LH && CONFIG(1);
    }
    else
    {
        //Get pressed buttons
        u32 pressed = HID_PAD;

        //If no configuration file exists or SELECT is held, load configuration menu
        if(needConfig == CREATE_CONFIGURATION || ((pressed & BUTTON_SELECT) && !(pressed & BUTTON_L1)))
        {
            configureCFW(configPath);

            //Zero the last booted FIRM flag
            CFG_BOOTENV = 0;

            nbChronoStarted = 1;
            chrono(0);
            chrono(2);

            //Update pressed buttons
            pressed = HID_PAD;
        }

        isFirmlaunch = false;
        firmType = NATIVE_FIRM;
        
        //Determine if booting with A9LH
        bool a9lhBoot = !PDN_SPI_CNT;

        //Determine if A9LH is installed and the user has an updated sysNAND
        if(a9lhBoot || CONFIG(2))
        {
            a9lhMode = A9LH_WITH_NFIRM_FIRMPROT;
            updatedSys = CONFIG(1);
        }
        else
        {
            a9lhMode = NO_A9LH;
            updatedSys = false;
        }

        newConfig = (u32)a9lhMode << 3;

        if(a9lhBoot)
        {
            //If it's a MCU reboot, try to force boot options
            if(CFG_BOOTENV)
            {
                //Always force a sysNAND boot when quitting AGB_FIRM
                if(CFG_BOOTENV == 7)
                {
                    nandType = FIRMWARE_SYSNAND;
                    firmSource = updatedSys ? FIRMWARE_SYSNAND : (FirmwareSource)BOOTCONFIG(2, 1);
                    needConfig = DONT_CONFIGURE;

                    //Flag to prevent multiple boot options-forcing
                    newConfig |= 1 << 4;
                }

                /* Else, force the last used boot options unless a button is pressed
                    or the no-forcing flag is set */
                else if(!pressed && !BOOTCONFIG(4, 1))
                {
                    nandType = (FirmwareSource)BOOTCONFIG(0, 3);
                    firmSource = (FirmwareSource)BOOTCONFIG(2, 1);
                    needConfig = DONT_CONFIGURE;
                }
            }

            //If the SAFE MODE combo is held, force a sysNAND boot
            else if(pressed == SAFE_MODE)
            {
                a9lhMode = A9LH_WITH_SFIRM_FIRMPROT;
                nandType = FIRMWARE_SYSNAND;
                firmSource = FIRMWARE_SYSNAND;
                needConfig = DONT_CONFIGURE;
            }
        }

        //Boot options aren't being forced
        if(needConfig != DONT_CONFIGURE)
        {
            /* If L and R/A/Select or one of the single payload buttons are pressed,
               chainload an external payload */
            if(DEVMODE || (pressed & SINGLE_PAYLOAD_BUTTONS) || ((pressed & BUTTON_L1) && (pressed & L_PAYLOAD_BUTTONS)))
                loadPayload(pressed);

            //If screens are inited or the corresponding option is set, load splash screen
            if((PDN_GPU_CNT != 1 || CONFIG(7)) && loadSplash())
            {
                nbChronoStarted = 2;
                chrono(0);
            }

            //If R is pressed, boot the non-updated NAND with the FIRM of the opposite one
            if(pressed & BUTTON_R1)
            {
                nandType = (updatedSys) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                firmSource = (updatedSys) ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND;
            }

            /* Else, boot the NAND the user set to autoboot or the opposite one, depending on L,
               with their own FIRM */
            else
            {
                nandType = (CONFIG(0) != !(pressed & BUTTON_L1)) ? FIRMWARE_EMUNAND : FIRMWARE_SYSNAND;
                firmSource = nandType;
            }

            /* If we're booting emuNAND the second emuNAND is set as default and B isn't pressed,
               or vice-versa, boot the second emuNAND */
            if(nandType != FIRMWARE_SYSNAND && (CONFIG(3) == !(pressed & BUTTON_B))) nandType = FIRMWARE_EMUNAND2;
        }
    }

    //If we need to boot emuNAND, make sure it exists
    if(nandType != FIRMWARE_SYSNAND)
    {
        locateEmuNAND(&emuOffset, &emuHeader, &nandType);
        if(nandType == FIRMWARE_SYSNAND) firmSource = FIRMWARE_SYSNAND;
    }

    //Same if we're using emuNAND as the FIRM source
    else if(firmSource != FIRMWARE_SYSNAND)
        locateEmuNAND(&emuOffset, &emuHeader, &firmSource);

    if(!isFirmlaunch)
    {
        newConfig |= (u32)nandType | ((u32)firmSource << 2);

        /* If the boot configuration is different from previously, overwrite it.
           Just the no-forcing flag being set is not enough */
        if((newConfig & 0x2F) != (config & 0x3F))
        {
            //Preserve user settings (last 26 bits)
            newConfig |= config & 0xFFFFFFC0;

            fileWrite(&newConfig, configPath, 4);
        }
    }

    loadFirm(firmType, firmType == NATIVE_FIRM && firmSource == ((updatedSys) ? FIRMWARE_SYSNAND : FIRMWARE_EMUNAND));
    
    switch(firmType)
    {
        case NATIVE_FIRM:
            patchNativeFirm(nandType, emuHeader, a9lhMode);
            break;
        case SAFE_FIRM:
            patchSafeFirm();
            break;
        default:
            patchLegacyFirm(firmType);
            break;
    }

    if(nbChronoStarted)
    {
        if(nbChronoStarted == 2) chrono(3);
        stopChrono();
    }

    launchFirm(firmType, isFirmlaunch);
}

static inline void loadFirm(FirmwareType firmType, bool externalFirm)
{
    section = firm->section;

    bool externalFirmLoaded = externalFirm &&
                              fileRead(firm, "/luma/firmware.bin") &&
                              (((u32)section[2].address >> 8) & 0xFF) == (isN3DS ? 0x60 : 0x68);

    /* If the conditions to load the external FIRM aren't met, or reading fails, or the FIRM
       doesn't match the console, load FIRM from CTRNAND */
    if(!externalFirmLoaded)
    {
        const char *firmFolders[4][2] = {{ "00000002", "20000002" },
                                         { "00000102", "20000102" },
                                         { "00000202", "20000202" },
                                         { "00000003", "20000003" }};

        firmRead(firm, firmFolders[(u32)firmType][isN3DS ? 1 : 0]);
        decryptExeFs((u8 *)firm);
    }
}

static inline void patchNativeFirm(FirmwareSource nandType, u32 emuHeader, A9LHMode a9lhMode)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset,
       *arm11Section1 = (u8 *)firm + section[1].offset;

    bool is90Firm;

    if(isN3DS)
    {
        u32 a9lVersion;
        
        //Determine the NATIVE_FIRM/arm9loader version
        switch(arm9Section[0x53])
        {
            case 0xFF:
                a9lVersion = 0;
                break;
            case '1':
                a9lVersion = 1;
                break;
            default:
                a9lVersion = 2;
                break;
        }

        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, a9lVersion);
        firm->arm9Entry = (u8 *)0x801B01C;
        is90Firm = a9lVersion == 0;
    }
    else
    {
        //Determine if we're booting the 9.0 FIRM
        u8 firm90Hash[0x10] = {0x27, 0x2D, 0xFE, 0xEB, 0xAF, 0x3F, 0x6B, 0x3B, 0xF5, 0xDE, 0x4C, 0x41, 0xDE, 0x95, 0x27, 0x6A};
        is90Firm = memcmp(section[2].hash, firm90Hash, 0x10) == 0;
    }

    //Find the Process9 .code location, size and memory address
    u32 process9Size,
        process9MemAddr;
    u8 *process9Offset = getProcess9(arm9Section + 0x15000, section[2].size - 0x15000, &process9Size, &process9MemAddr);

    //Apply signature patches
    patchSignatureChecks(process9Offset, process9Size);

    //Apply emuNAND patches
    if(nandType != FIRMWARE_SYSNAND)
    {
        u32 branchAdditive = (u32)firm + section[2].offset - (u32)section[2].address;
        patchEmuNAND(arm9Section, section[2].size, process9Offset, process9Size, emuOffset, emuHeader, branchAdditive);
    }

    //Apply FIRM0/1 writes patches on sysNAND to protect A9LH
    else if(a9lhMode != NO_A9LH) patchFirmWrites(process9Offset, process9Size);

    //Apply firmlaunch patches, not on 9.0 FIRM as it breaks firmlaunchhax
    if(!is90Firm || a9lhMode == A9LH_WITH_SFIRM_FIRMPROT) patchFirmlaunches(process9Offset, process9Size, process9MemAddr);

    if(!is90Firm)
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

static inline void patchLegacyFirm(FirmwareType firmType)
{
    u8 *arm9Section = (u8 *)firm + section[3].offset;
    
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(isN3DS)
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

    applyLegacyFirmPatches((u8 *)firm, firmType, isN3DS);
}

static inline void patchSafeFirm(void)
{
    u8 *arm9Section = (u8 *)firm + section[2].offset;

    if(isN3DS)
    {
        //Decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
        arm9Loader(arm9Section, 0);
        firm->arm9Entry = (u8 *)0x801B01C;

        patchFirmWrites(arm9Section, section[2].size);
    }
    else patchFirmWriteSafe(arm9Section, section[2].size);
    
    if(DEVMODE)
    {
        //Kernel9/Process9 debugging
        patchExceptionHandlersInstall(arm9Section, section[2].size);
        patchSvcBreak9(arm9Section, section[2].size, (u32)(section[2].address));
    }
}

static inline void copySection0AndInjectSystemModules(void)
{
    u8 *arm11Section0 = (u8 *)firm + section[0].offset;
    char fileName[] = "/luma/sysmodules/--------.cxi";
    const char *ext = ".cxi";

    struct
    {
        u32 size;
        char name[8];
        const u8 *addr;
    } modules[5] = {{0}};

    u8 *pos = arm11Section0;
    u32 loaderIndex = 0;
    for(u32 i = 0; i < 5; i++)
    {
        modules[i].addr = pos;
        modules[i].size = *(u32 *)(pos + 0x104) * 0x200;
        
        memcpy(modules[i].name, pos + 0x200, 8);
        pos += modules[i].size;
        
        //Read modules from files if they exist
        u32 nameOff;
        for(nameOff = 0; nameOff < 8 && modules[i].name[nameOff] != 0; nameOff++);
        memcpy(fileName + 17, modules[i].name, nameOff);
        memcpy(fileName + 17 + nameOff, ext, 5);
        
        u32 fileSize = getFileSize(fileName);
        if(fileSize != 0)
        {
            modules[i].addr = NULL;
            modules[i].size = fileSize;
        }

        if(memcmp(modules[i].name, "loader", 7) == 0) loaderIndex = i;
    }

    if(modules[loaderIndex].addr != NULL)
    {
        modules[loaderIndex].size = injector_size;
        modules[loaderIndex].addr = injector;
    }

    pos = section[0].address;
    for(u32 i = 0; i < 5; i++)
    {
        if(modules[i].addr != NULL)
            memcpy(pos, modules[i].addr, modules[i].size);
        else
        {
             //Read modules from files if they exist
            u32 nameOff;
            for(nameOff = 0; nameOff < 8 && modules[i].name[nameOff] != 0; nameOff++);
            memcpy(fileName + 17, modules[i].name, nameOff);
            memcpy(fileName + 17 + nameOff, ext, 5);
            fileRead(pos, fileName);
        }

        pos += modules[i].size;
    }
}

static inline void launchFirm(FirmwareType firmType, bool isFirmlaunch)
{
    //If we're booting NATIVE_FIRM, section0 needs to be copied separately to inject 3ds_injector
    u32 sectionNum;
    if(firmType == NATIVE_FIRM)
    {
        copySection0AndInjectSystemModules();
        sectionNum = 1;
    }
    else sectionNum = 0;

    //Copy FIRM sections to respective memory locations
    for(; sectionNum < 4 && section[sectionNum].size; sectionNum++)
        memcpy(section[sectionNum].address, (u8 *)firm + section[sectionNum].offset, section[sectionNum].size);

    //Determine the ARM11 entry to use
    vu32 *arm11;
    if(isFirmlaunch) arm11 = (u32 *)0x1FFFFFFC;
    else
    {
        deinitScreens();
        arm11 = (u32 *)0x1FFFFFF8;
    }

    flushEntireDCache(); //Ensure that all memory transfers have completed and that the data cache has been flushed 
    flushEntireICache();

    //Set ARM11 kernel entrypoint
    *arm11 = (u32)firm->arm11Entry;

    //Final jump to ARM9 kernel
    ((void (*)())firm->arm9Entry)();
}