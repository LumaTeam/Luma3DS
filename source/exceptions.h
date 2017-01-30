/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
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
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

#pragma once

#include "types.h"

#define MAKE_BRANCH(src,dst)      (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define MAKE_BRANCH_LINK(src,dst) (0xEB000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

void installArm9Handlers(void);
u32 installArm11Handlers(u32 *exceptionsPage, u32 stackAddress, u32 codeSetOffset, u32 *dAbtHandler, u32 dAbtHandlerMemAddress);
void detectAndProcessExceptionDumps(void);
