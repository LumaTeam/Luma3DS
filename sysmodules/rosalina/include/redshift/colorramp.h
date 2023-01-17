/* colorramp.h -- color temperature calculation header
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

   Copyright (c) 2010-2014  Jon Lund Steffensen <jonlst@gmail.com>
*/

#ifndef REDSHIFT_COLORRAMP_H
#define REDSHIFT_COLORRAMP_H

#include <stdint.h>

//#include "redshift.h"

void colorramp_get_white_point(float *out_white_point, int temperature); // not in original code

#endif /* ! REDSHIFT_COLORRAMP_H */
