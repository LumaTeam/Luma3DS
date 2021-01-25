/*
*   This file is part of Luma3DS
*   Copyright (C) 2021 Aurora Wright, TuxSH
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

#include "types.h"

typedef struct GpioRegisters {
    u8 gpio1_data;
    u8 _0x001[0x010 - 0x001];

    u8 gpio2_data;
    u8 gpio2_dir;
    u8 gpio2_intcfg;
    u8 gpio2_inten;
    u16 gpio2_data2;
    u8 _0x016[0x020 - 0x016];

    u16 gpio3_data;
    u16 gpio3_dir;
    u16 gpio3_intcfg;
    u16 gpio3_inten;
    u16 gpio3_data2;
    u8 _0x02a[0x100 - 0x02A];
} GpioRegisters;

typedef enum GpioDirection {
    GPIO_DIR_INPUT  = 0,
    GPIO_DIR_OUTPUT = 1,
} GpioDirection;

typedef enum GpioInterruptConfig {
    GPIO_INTCFG_FALLING_EDGE    = 0,
    GPIO_INTCFG_RISING_EDGE     = 1,
} GpioInterruptConfig;

#define GPIO_PIN(bank, idx) (((bank) << 4) | ((idx) & 0xF))
#define BANK_OF(pin)        ((pin) >> 4)
#define BIT_OF(pin)         ((pin) & 0xF)

typedef enum GpioPin {
    // GPIO1
    GPIO_DEBUG_BUTTON           = GPIO_PIN(1, 0), // active-low
    GPIO_TOUCH_SCREEN           = GPIO_PIN(1, 1), // active-low, 0 when the touch screen is pressed
    GPIO_SHELL_CLOSED           = GPIO_PIN(1, 2),

    // GPIO2
    GPIO_HEADPHONES_INSERTED    = GPIO_PIN(2, 0),
    GPIO_TWL_DEPOP              = GPIO_PIN(2, 1), // active-low

    // GPIO2 (DATA2)
    GPIO_WIFI_MODE              = GPIO_PIN(4, 0), // 0 is CTR, 1 is MP (DS WiFi)

    // GPIO3
    GPIO_CSTICK_INT             = GPIO_PIN(3, 0),
    GPIO_IRDA_INT               = GPIO_PIN(3, 1), // active-low
    GPIO_GYRO_INT               = GPIO_PIN(3, 2),
    GPIO_CSTICK_STOP            = GPIO_PIN(3, 3), // output
    GPIO_IRDA_TXRC              = GPIO_PIN(3, 4), // output
    GPIO_IRDA_RXD               = GPIO_PIN(3, 5), // active-low
    GPIO_NFC_OUT1               = GPIO_PIN(3, 6), // output
    GPIO_NFC_OUT2               = GPIO_PIN(3, 7), // output
    GPIO_HEADPHONES_BUTTON      = GPIO_PIN(3, 8), // active-low ("half-inserted")
    GPIO_MCU_INT                = GPIO_PIN(3, 9),
    GPIO_NFC_INT                = GPIO_PIN(3, 10),
    GPIO_QTM_OUT                = GPIO_PIN(3, 11), // output

    // GPIO3 (DATA2)
    GPIO_WIFI_ENABLED           = GPIO_PIN(5, 0),
} GpioPin;

static volatile GpioRegisters *const GPIO = (volatile GpioRegisters *)0x10147000;

static inline bool gpioRead(GpioPin pin)
{
    u32 bank = BANK_OF(pin);
    u32 bit = BIT_OF(pin);

    switch (bank) {
        case 1:
            return (GPIO->gpio1_data & BIT(bit)) != 0;
        case 2:
            return (GPIO->gpio2_data & BIT(bit)) != 0;
        case 3:
            return (GPIO->gpio3_data & BIT(bit)) != 0;
        case 4:
            return (GPIO->gpio2_data2 & BIT(bit)) != 0;
        case 5:
            return (GPIO->gpio3_data2 & BIT(bit)) != 0;

        default:
            return false;
    }
}

static inline void gpioWrite(GpioPin pin, bool val)
{
    u32 bank = BANK_OF(pin);
    u32 bit = BIT_OF(pin);

    u32 valMask = (val ? 1 : 0) << bit;
    u32 tmp;

    switch (bank) {
        case 1:
            tmp = GPIO->gpio1_data & ~BIT(bit);
            GPIO->gpio1_data = (u8)(tmp | valMask);
            break;
        case 2:
            tmp = GPIO->gpio2_data & ~BIT(bit);
            GPIO->gpio2_data = (u8)(tmp | valMask);
            break;
        case 3:
            tmp = GPIO->gpio3_data & ~BIT(bit);
            GPIO->gpio3_data = (u16)(tmp | valMask);
            break;
        case 4:
            tmp = GPIO->gpio2_data2 & ~BIT(bit);
            GPIO->gpio2_data2 = (u16)(tmp | valMask);
            break;
        case 5:
            tmp = GPIO->gpio3_data2 & ~BIT(bit);
            GPIO->gpio3_data2 = (u16)(tmp | valMask);
            break;

        default:
            break;
    }
}

static inline void gpioSetDirection(GpioPin pin, GpioDirection direction)
{
    u32 bank = BANK_OF(pin);
    u32 bit = BIT_OF(pin);

    u32 valMask = (direction == GPIO_DIR_OUTPUT ? 1 : 0) << bit;
    u32 tmp;

    switch (bank) {
        case 2:
            tmp = GPIO->gpio2_dir & ~BIT(bit);
            GPIO->gpio2_dir = (u8)(tmp | valMask);
            break;
        case 3:
            tmp = GPIO->gpio3_dir & ~BIT(bit);
            GPIO->gpio3_dir = (u16)(tmp | valMask);
            break;

        default:
            break;
    }
}

static inline void gpioConfigureInterrupt(GpioPin pin, GpioInterruptConfig cfg)
{
    u32 bank = BANK_OF(pin);
    u32 bit = BIT_OF(pin);

    u32 valMask = (cfg == GPIO_INTCFG_RISING_EDGE ? 1 : 0) << bit;
    u32 tmp;

    switch (bank) {
        case 2:
            tmp = GPIO->gpio2_intcfg & ~BIT(bit);
            GPIO->gpio2_intcfg = (u8)(tmp | valMask);
            break;
        case 3:
            tmp = GPIO->gpio3_data & ~BIT(bit);
            GPIO->gpio3_intcfg = (u16)(tmp | valMask);
            break;

        default:
            break;
    }
}

static inline void gpioSetInterruptEnabled(GpioPin pin, bool enabled)
{
    u32 bank = BANK_OF(pin);
    u32 bit = BIT_OF(pin);

    u32 valMask = (enabled ? 1 : 0) << bit;
    u32 tmp;

    switch (bank) {
        case 2:
            tmp = GPIO->gpio2_inten & ~BIT(bit);
            GPIO->gpio2_inten = (u8)(tmp | valMask);
            break;
        case 3:
            tmp = GPIO->gpio3_inten & ~BIT(bit);
            GPIO->gpio3_inten = (u16)(tmp | valMask);
            break;

        default:
            break;
    }
}

#undef GPIO_PIN
#undef BIT_OF
#undef BANK_OF
