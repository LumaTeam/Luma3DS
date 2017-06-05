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

#include <3ds.h>
#include "fmt.h"
#include "menus/power.h"
#include "memory.h"
#include "menu.h"


Menu PowerMenu = {
    "Power menu",
    .nbItems = 2,
    {
        { "Power off 3DS", METHOD, .method = &PoweroffMenu },
        { "Reboot 3DS", METHOD, .method = &RebootMenu }
    }
};

void RebootMenu(void)
{
svcKernelSetState(7);
}

void PoweroffMenu(void)
{
    Handle nssHandle = 0;
    Result result = srvGetServiceHandle(&nssHandle, "ns:s");
    if (result != 0)
        return;

    u32 *commandBuffer = getThreadCommandBuffer();
    commandBuffer[0] = 0x000E0000;

    svcSendSyncRequest(nssHandle);
    svcCloseHandle(nssHandle);
 

}
