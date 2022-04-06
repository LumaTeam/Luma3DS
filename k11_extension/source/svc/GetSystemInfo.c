/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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

#include "svc/GetSystemInfo.h"
#include "utils.h"
#include "ipc.h"
#include "synchronization.h"

Result GetSystemInfoHook(s64 *out, s32 type, s32 param)
{
    Result res = 0;

    switch(type)
    {
        case 0x10000:
        {
            switch(param)
            {
                case 0:
                    *out = SYSTEM_VERSION(cfwInfo.versionMajor, cfwInfo.versionMinor, cfwInfo.versionBuild);
                    break;
                case 1:
                    *out = cfwInfo.commitHash;
                    break;
                case 2:
                    *out = (cfwInfo.configFormatVersionMajor << 16) | cfwInfo.configFormatVersionMinor;
                    break;
                case 3:
                    *out = cfwInfo.config;
                    break;
                case 4:
                    *out = cfwInfo.multiConfig;
                    break;
                case 5:
                    *out = cfwInfo.bootConfig;
                    break;
                case 6:
                    *out = cfwInfo.splashDurationMsec;
                    break;
                case 0x80:
                    *out = fcramDescriptor->appRegion.regionSizeInBytes;
                    break;
                case 0x100:
                    *out = (s64)cfwInfo.hbldr3dsxTitleId;
                    break;
                case 0x101:
                    *out = cfwInfo.rosalinaMenuCombo;
                    break;
                case 0x102:
                    *out = cfwInfo.screenFiltersCct;
                    break;
                case 0x103:
                    *out = (s64)cfwInfo.ntpTzOffetMinutes;
                    break;
                case 0x180:
                    *out = cfwInfo.pluginLoaderFlags;
                    break;
                case 0x200: // isRelease
                    *out = cfwInfo.flags & 1;
                    break;
                case 0x201: // isN3DS
                    *out = (cfwInfo.flags >> 4) & 1;
                    break;
                case 0x202: // needToInitSd
                    *out = (cfwInfo.flags >> 5) & 1;
                    break;
                case 0x203: // isSdMode
                    *out = (cfwInfo.flags >> 6) & 1;
                    break;

                case 0x300: // K11Ext size
                    *out = (s64)(((u64)kextBasePa << 32) | (u64)(__end__ - __start__));
                    break;

                case 0x301: // stolen SYSTEM memory size
                    *out = stolenSystemMemRegionSize;
                    break;

                default:
                    *out = 0;
                    res = 0xF8C007F4; // not implemented
                    break;
            }
            break;
        }

        case 0x10001: // N3DS-related info
        {
            if(isN3DS)
            {
                switch(param)
                {
                    case 0: // current clock rate
                        *out = (((CFG11_MPCORE_CLKCNT >> 1) & 3) + 1) * 268;
                        break;
                    case 1: // higher clock rate
                        *out = (((CFG11_MPCORE_CFG >> 2) & 1) + 2) * 268;
                        break;
                    case 2: // L2C enabled status
                        *out = L2C_CTRL & 1;
                        break;
                    default:
                        *out = 0;
                        res = 0xF8C007F4;
                        break;
                }
            }
            else
            {
                *out = 0;
                res = 0xF8C007F4;
            }
            break;
        }

        case 0x10002: // MMU config (cached values from booting)
        {
            switch(param)
            {
                case 0:
                    *out = TTBCR;
                    break;

                default:
                {
                    if((u32)param <= getNumberOfCores())
                        *out = L1MMUTableAddrs[param - 1];
                    else
                    {
                        *out = 0;
                        res = 0xF8C007F4;
                    }

                    break;
                }
            }

            break;
        }

        case 0x20000:
        {
            *out = 0;
            return 1;
        }

        default:
            GetSystemInfo(out, type, param);
            break;
    }

    return res;
}
