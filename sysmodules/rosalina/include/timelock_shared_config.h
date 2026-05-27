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

#define PIN_LENGTH        4
#define CONFIG_FILE_PATH  "/luma/timelock_config.bin"

// Versioning: any config with a different magic/version is treated as
// missing and re-created with defaults. Avoids any migration code while
// the schema is still evolving.
#define TIMELOCK_MAGIC    0x4B434C54u    // 'TLCK'
#define TIMELOCK_VERSION  2u

#define TIMELOCK_HASH_LEN 32             // SHA-256
#define TIMELOCK_SALT_LEN 16             // per-install random salt

// PIN is "0000" by default. Stored only as SHA-256(salt || pin); the
// plaintext never lives in the config file.
extern const char PIN_ZERO[PIN_LENGTH];

typedef struct CTR_PACKED CTR_ALIGN(4) timelockConfig
{
    u32  magic;                          // TIMELOCK_MAGIC
    u16  version;                        // TIMELOCK_VERSION
    u16  minutes;                        // limit before lock fires
    u16  elapsedMinutes;                 // current elapsed
    bool isEnabled;
    bool dailyResetEnabled;              // auto-reset elapsed every day at resetHourLocal
    u8   resetHourLocal;                 // 0-23
    u8   reserved0;
    u8   pinHash[TIMELOCK_HASH_LEN];     // SHA-256(salt || pin)
    u8   pinSalt[TIMELOCK_SALT_LEN];     // per-install random; generated once at init
    s64  nextResetEligibleAtMs;          // RTC ms since 2000-01-01; only credit a daily reset when now >= this
} timelockConfig;


extern bool isRosalinaMenuOpened;
extern bool isTimeLocked;
