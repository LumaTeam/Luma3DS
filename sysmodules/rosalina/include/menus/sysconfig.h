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

#pragma once

#include <3ds/types.h>
#include "menu.h"

extern Menu sysconfigMenu;
extern bool isConnectionForced;
extern s8 currVolumeSliderOverride;

void SysConfigMenu_UpdateStatus(bool control);

void SysConfigMenu_ToggleLEDs(void);
void SysConfigMenu_ToggleWireless(void);
void togglePowerButton(void);
void SysConfigMenu_TogglePowerButton(void);
void SysConfigMenu_ControlWifi(void);
void SysConfigMenu_DisableForcedWifiConnection(void);
void SysConfigMenu_ToggleCardIfPower(void);
void SysConfigMenu_LoadConfig(void);
void SysConfigMenu_AdjustVolume(void);
void SysConfigMenu_ChangeScreenBrightness(void);
