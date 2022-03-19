/*   This paricular file is licensed under the following terms: */

/*
*   This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable
*   for any damages arising from the use of this software.
*
*   Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it
*   and redistribute it freely, subject to the following restrictions:
*
*    The origin of this software must not be misrepresented; you must not claim that you wrote the original software.
*    If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
*
*    Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
*    This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include <3ds/types.h>

#define PIN_LENGTH       4
#define CONFIG_FILE_PATH "/luma/timelock_config.bin"


typedef struct PACKED ALIGN(4) timelockConfig
{
    bool isEnabled;
    u16 minutes;
    char pin[PIN_LENGTH];
    u16 elapsedMinutes;
} timelockConfig;


extern bool isRosalinaMenuOpened;
extern bool isTimeLocked;
