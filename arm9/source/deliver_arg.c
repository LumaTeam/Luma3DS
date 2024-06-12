/*
*   This file is part of Luma3DS
*   Copyright (C) 2022 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "deliver_arg.h"
#include "utils.h"
#include "memory.h"
#include "config.h"
#include "fs.h"
#include "i2c.h"

u8 *loadDeliverArg(void)
{
    static __attribute__((aligned(8))) u8 deliverArg[0x1000] = {0};
    static bool deliverArgLoaded = false;

    if (!deliverArgLoaded)
    {
        u32 bootenv = CFG_BOOTENV;  // this register is preserved across reboots
        if ((bootenv & 1) == 0) // true coldboot
        {
            memset(deliverArg, 0, 0x1000);
        }
        else
        {
            u32 mode = bootenv >> 1;
            if (mode == 0) // CTR mode
            {
                memcpy(deliverArg, (const void *)0x20000000, 0x1000);

                // Validate deliver arg
                u32 testPattern = *(u32 *)(deliverArg + 0x438);
                u32 *crcPtr = (u32 *)(deliverArg + 0x43C);
                u32 crc = *crcPtr;
                *crcPtr = 0; // clear crc field before calculation
                u32 expectedCrc = crc32(deliverArg + 0x400, 0x140, 0xFFFFFFFF);
                *crcPtr = crc;
                if (testPattern != 0xFFFF || crc != expectedCrc)
                    memset(deliverArg, 0, 0x1000);
            }
            else // Legacy modes
            {
                // Copy TWL deliver arg stuff as-is (0...0x300)
                copyFromLegacyModeFcram(deliverArg, (const void *)0x20000000, 0x400);

                // Validate TLNC (TWL launcher params) block
                // Note: Nintendo doesn't do crcLen bound check
                u8 *tlnc = deliverArg + 0x300;
                bool hasMagic = memcmp(tlnc, "TLNC", 4) == 0;
                u8 crcLen = tlnc[5];
                u16 crc = *(u16 *)(tlnc + 6);
                if (!hasMagic || crcLen <= 248 || crc != crc16(tlnc + 8, crcLen, 0xFFFF))
                    memset(tlnc, 0, 0x100);

                memset(deliverArg + 0x400, 0, 0xC00);
            }
        }
        deliverArgLoaded = true;
    }

    return deliverArg;
}

void commitDeliverArg(void)
{
    u8 *deliverArg = loadDeliverArg();
    u32 bootenv = CFG_BOOTENV;

    if ((bootenv & 1) == 0) // if true coldboot, set bootenv to "CTR mode reboot"
    {
        bootenv = 1;
        CFG_BOOTENV = 1;
    }

    u32 mode = bootenv >> 1;
    if (mode == 0) // CTR mode
    {
        *(u32 *)(deliverArg + 0x438) = 0xFFFF;
        *(u32 *)(deliverArg + 0x43C) = 0; // clear CRC field before calculating it
        *(u32 *)(deliverArg + 0x43C) = crc32(deliverArg + 0x400, 0x140, 0xFFFFFFFF);
        memcpy((void *)0x20000000, deliverArg, 0x1000);
    }
    else // Legacy modes (just TWL mode, really)
    {
        copyToLegacyModeFcram((void *)0x20000000, deliverArg, 0x400);
    }
}

bool hasValidTlncAutobootParams(void)
{
    u8 *tlnc = loadDeliverArg() + 0x300; // loadDeliverArg clears invalid TLNC blocks
    return memcmp(tlnc, "TLNC", 4) == 0 && (*(u16 *)(tlnc + 0x18) & 1) != 0;
}

bool isTwlToCtrLaunch(void)
{
    // assumes TLNC block is valid
    u8 *tlnc = loadDeliverArg() + 0x300; // loadDeliverArg clears invalid TLNC blocks
    u64 twlTid = *(u64 *)(tlnc + 0x10);

    switch (twlTid & ~0xFFull)
    {
        case 0x0000000000000000ull: // TWL Launcher -> Home menu (note: NS checks full TID)
        case 0x00030015484E4200ull: // TWL System Settings -> CTR System Settings (mset)
            return true;
        default:
            return false;
    }
}

static bool configureHomebrewAutobootCtr(u8 *deliverArg)
{
    static const u8 appmemtypesO3ds[] = { 0, 2, 3, 4, 5 };
    static const u8 appmemtypesN3ds[] = { 6, 7, 7, 7, 7 };

    u64 hbldrTid = configData.hbldr3dsxTitleId;
    hbldrTid = hbldrTid == 0 ? HBLDR_DEFAULT_3DSX_TID : hbldrTid; // replicate Loader's behavior
    if ((hbldrTid >> 46) != 0x10) // Not a CTR titleId. Bail out
        return false;

    u8 memtype = configData.autobootCtrAppmemtype;
    deliverArg[0x400] = ISN3DS ? appmemtypesN3ds[memtype] : appmemtypesO3ds[memtype];

    // Determine whether to load from the SD card or from NAND. We don't support gamecards for this
    u32 category = (hbldrTid >> 32) & 0xFFFF;
    bool isSdApp = (category & 0x10) == 0 && category != 1; // not a system app nor a DLP child
    *(u64 *)(deliverArg + 0x440) = hbldrTid;
    *(u64 *)(deliverArg + 0x448) = isSdApp ? 1 : 0;

    // Tell NS to run the title, and that it's not a title jump from legacy mode
    *(u32 *)(deliverArg + 0x460) = (0 << 1) | (1 << 0);

    // Whenever power button is held long enough ("force shutdown"), mcu sysmodule
    // stores a flag in free reg 0. It will clear it next boot.

    // During that next boot, if that flag was set and if CFG_BOOTENV.bit0 is set
    // (warmboot/firm chainload, i.e. not coldbooting), then main() will simulate
    // a "power button held" interrupt (after upgrading mcu fw if necessary -- it
    // will reboot console after if it has upgraded mcu fw, I guess that's one of
    // the reasons the flag is there). This obviously cause other processes to initiate
    // a shutdown, and it also sets that flag again.

    // In the case of autoboot, ns will panic when this happens. This caused
    // hb autoboot to keep failing over and over again.

    // Select free reg 0, read it, select it again, write it (clearing force shutdown flag)
    I2C_writeReg(I2C_DEV_MCU, 0x60, 0);
    u8 flags = I2C_readReg(I2C_DEV_MCU, 0x61);
    flags &= ~4;
    I2C_writeReg(I2C_DEV_MCU, 0x60, 0);
    I2C_writeReg(I2C_DEV_MCU, 0x61, flags);

    CFG_BOOTENV = 1;

    return true;
}

static bool configureHomebrewAutobootTwl(u8 *deliverArg)
{
    // Here, we pretend to be a TWL app rebooting into another TWL app.
    // We get NS to do all the heavy lifting (starting NWM and AM, etc.) this way.

    memset(deliverArg + 0x000, 0, 0x300); // zero TWL deliver arg params

    // Now onto TLNC (launcher params):
    u8 *tlnc = deliverArg + 0x300;
    memset(tlnc, 0, 0x100);
    memcpy(tlnc, "TLNC", 4);
    tlnc[4] = 1; // version
    tlnc[5] = 0x18; // length of data to calculate CRC over

    *(u64 *)(tlnc + 8) = 0; // old title ID
    *(u64 *)(tlnc + 0x10) = configData.autobootTwlTitleId; // new title ID
    // bit4: "skip logo" ; bits2:1: NAND boot ; bit0: valid
    *(u16 *)(tlnc + 0x18) = (1 << 4) | (3 << 1) | (1 << 0);

    *(u16 *)(tlnc + 6) = crc16(tlnc + 8, 0x18, 0xFFFF);

    CFG_BOOTENV = 3;

    return true;
}

bool configureHomebrewAutoboot(void)
{
    bool ret;
    u8 *deliverArg = loadDeliverArg();

    u32 bootenv = CFG_BOOTENV;
    u32 mode = bootenv >> 1;

    // NS always writes a valid deliver arg on reboot, no matter what.
    // Check if it is empty, and, of course, bail out if we aren't rebooting from
    // NATIVE_FIRM.
    // Checking if it is empty is necessary to let us reboot from autobooted hbmenu
    // to hbmenu.

    if (mode != 0)
        return false;
    else if (bootenv == 1)
    {
        for (u32 i = 0; i < 0x410; i++)
        {
            if (deliverArg[i] != 0)
                return false;
        }
        for (u32 i = 0x440; i < 0x1000; i++)
        {
            if (deliverArg[i] != 0)
                return false;
        }
    }

    switch (MULTICONFIG(AUTOBOOTMODE))
    {
        case 1:
            ret = configureHomebrewAutobootCtr(deliverArg);
            break;
        case 2:
            ret = configureHomebrewAutobootTwl(deliverArg);
            break;
        case 0:
        default:
            ret = false;
            break;
    }

    if (ret)
        commitDeliverArg();
    return ret;
}
