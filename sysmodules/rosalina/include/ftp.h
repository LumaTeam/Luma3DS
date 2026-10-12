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
*
* Written by Saiitanaa
*/

#pragma once

#include <3ds/types.h>

#define FTP_CONTROL_PORT 5000

#define FTP_ERR_NO_WIFI  ((Result)-0x1001)
#define FTP_ERR_SOCKET   ((Result)-0x1002)

typedef enum FtpState
{
    FTP_STATE_STOPPED = 0,
    FTP_STATE_STARTING,
    FTP_STATE_RUNNING,
    FTP_STATE_ERROR
} FtpState;

void     ftpStart(void);
void     ftpStop(void);
FtpState ftpGetState(void);
u32      ftpGetIp(void);
u32 ftpGetAcceptCount(void);
Result   ftpGetError(void);