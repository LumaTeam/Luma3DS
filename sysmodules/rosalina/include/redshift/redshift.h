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

void Redshift_EditableFilter();
void Redshift_ApplySavedFilter();
void Redshift_SuppressLeds();

#endif /* ! REDSHIFT_REDSHIFT_H */