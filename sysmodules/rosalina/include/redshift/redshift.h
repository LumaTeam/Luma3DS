/* redshift.h -- Main program header
   This file is part of Redshift.

   Redshift is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Redshift is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Redshift.  If not, see <http://www.gnu.org/licenses/>.

   Copyright (c) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifndef REDSHIFT_REDSHIFT_H
#define REDSHIFT_REDSHIFT_H

#include <stdio.h>
#include <stdlib.h>
#include <3ds/types.h>

/* The color temperature when no adjustment is applied. */
#define NEUTRAL_TEMP  6500

#define MIN_TEMP 1000
#define MAX_TEMP 25000
#define MIN_GAMMA 0.10
#define MAX_GAMMA 10.0
#define MIN_BRIGHTNESS 0.10
#define MAX_BRIGHTNESS 1.0

/* Color setting */
typedef struct {
	int temperature;
	float gamma[3];
	float brightness;
} color_setting_t;

extern bool nightLightSettingsRead;
extern bool nightLightOverride;

typedef struct {
   u8 abl_enabled;
   u8 brightness_level;
} backlight_controls;

typedef struct {
   u8 light_brightnessLevel;
   bool light_filterEnabled;
   bool light_ledSuppression;
   u8 light_startHour;
   u8 light_startMinute;
   u8 night_brightnessLevel;
   bool night_filterEnabled;
   bool night_ledSuppression; 
   u8 night_startHour;
   u8 night_startMinute;
   bool use_nightMode;
   bool light_changeBrightness;
   bool night_changeBrightness;
} night_light_settings;

void Redshift_EditableFilter(u8 filterNo);
void Redshift_ApplySavedFilter(u8 filterNo);
void Redshift_SuppressLeds();
bool Redshift_LcdsAvailable();
bool Redshift_ReadNightLightSettings();
void Redshift_ApplyNightLightSettings();
void ApplyLightMode();
void ApplyNightMode();
void Redshift_UpdateNightLightStatuses();
void Redshift_ConfigureNightLightSettings();

#endif /* ! REDSHIFT_REDSHIFT_H */
