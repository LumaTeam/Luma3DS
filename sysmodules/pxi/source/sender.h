/*
sender.h
	Handles commands from arm11 processes, then sends them to Process9, and replies to arm11 processes the replies received from Process9 (=> receiver.c) (except for PXISRV11).

(c) TuxSH, 2016-2020
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include "common.h"

Result sendPXICmdbuf(Handle *additionalHandle, u32 serviceId, u32 *buffer);
void sender(void);
void PXISRV11Handler(void);
