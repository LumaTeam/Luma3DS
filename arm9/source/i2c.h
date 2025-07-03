#pragma once

/*
 *   This file is part of fastboot 3DS
 *   Copyright (C) 2017 derrek, profi200
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
 */

#include <stdbool.h>
#include "types.h"


#define I2C_STOP          (1u)
#define I2C_START         (1u<<1)
#define I2C_ERROR         (1u<<2)
#define I2C_ACK           (1u<<4)
#define I2C_DIRE_WRITE    (0u)
#define I2C_DIRE_READ     (1u<<5)
#define I2C_IRQ_ENABLE    (1u<<6)
#define I2C_ENABLE        (1u<<7)

#define I2C_GET_ACK(reg)  ((bool)((reg)>>4 & 1u))


typedef enum
{
    I2C_DEV_POWER     = 0, 	// Unconfirmed
    I2C_DEV_CAMERA    = 1, 	// Unconfirmed
    I2C_DEV_CAMERA2   = 2, 	// Unconfirmed
    I2C_DEV_MCU       = 3,
    I2C_DEV_LCD_TOP   = 5,
    I2C_DEV_LCD_BOT   = 6,
    I2C_DEV_GYRO      = 10,
    I2C_DEV_DEBUG_PAD = 12,
    I2C_DEV_IR        = 13,
    I2C_DEV_EEPROM    = 14, // Unconfirmed
    I2C_DEV_NFC       = 15,
    I2C_DEV_QTM       = 16,
    I2C_DEV_N3DS_HID  = 17
} I2cDevice;



/**
 * @brief      Initializes the I2C buses. Call this only once.
 */
void I2C_init(void);

/**
 * @brief      Reads data from a I2C register to a buffer.
 *
 * @param[in]  devId    The device ID. Use the enum above.
 * @param[in]  regAddr  The register address.
 * @param      out      The output buffer pointer.
 * @param[in]  size     The read size.
 *
 * @return     Returns true on success and false on failure.
 */
bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size);

/**
 * @brief      Writes a buffer to a I2C register.
 *
 * @param[in]  devId    The device ID. Use the enum above.
 * @param[in]  regAddr  The register address.
 * @param[in]  in       The input buffer pointer.
 * @param[in]  size     The write size.
 *
 * @return     Returns true on success and false on failure.
 */
bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size);

/**
 * @brief      Reads a byte from a I2C register.
 *
 * @param[in]  devId    The device ID. Use the enum above.
 * @param[in]  regAddr  The register address.
 *
 * @return     Returns the value read on success otherwise 0xFF.
 */
u8 I2C_readReg(I2cDevice devId, u8 regAddr);

/**
 * @brief      Writes a byte to a I2C register.
 *
 * @param[in]  devId    The device ID. Use the enum above.
 * @param[in]  regAddr  The register address.
 * @param[in]  data     The data to write.
 *
 * @return     Returns true on success and false on failure.
 */
bool I2C_writeReg(I2cDevice devId, u8 regAddr, u8 data);
