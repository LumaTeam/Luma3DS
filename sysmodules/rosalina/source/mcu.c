/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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

#include "mcu.h"

Result mcuInit(void)
{
    return srvGetServiceHandle(&mcuhwcHandle, "mcu::HWC");
}

Result mcuExit(void)
{
    return svcCloseHandle(mcuhwcHandle);
}

Result mcuReadRegister(u8 reg, u8* data, u32 size)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x10082;
    ipc[1] = reg;
    ipc[2] = size;
    ipc[3] = size << 4 | 0xC;
    ipc[4] = (u32)data;
    Result ret = svcSendSyncRequest(mcuhwcHandle);
    if(ret < 0) return ret;
    return ipc[1];
}

Result mcuWriteRegister(u8 reg, u8* data, u32 size)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x20082;
    ipc[1] = reg;
    ipc[2] = size;
    ipc[3] = size << 4 | 0xA;
    ipc[4] = (u32)data;
    Result ret = svcSendSyncRequest(mcuhwcHandle);
    if(ret < 0) return ret;
    return ipc[1];
}

Result mcuGetLEDState(u8* out)
{
  return mcuReadRegister(0x28, out, 1);
}
