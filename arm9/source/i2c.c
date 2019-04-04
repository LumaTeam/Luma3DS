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
#include "i2c.h"
#include "utils.h"

#define I2C1_REGS_BASE  (0x10161000)

#define I2C2_REGS_BASE  (0x10144000)

#define I2C3_REGS_BASE  (0x10148000)


typedef struct
{
    vu8  REG_I2C_DATA;
    vu8  REG_I2C_CNT;
    vu16 REG_I2C_CNTEX;
    vu16 REG_I2C_SCL;
} I2cRegs;

static const struct
{
    u8 busId;
    u8 devAddr;
} i2cDevTable[] =
{
    {0, 0x4A},
    {0, 0x7A},
    {0, 0x78},
    {1, 0x4A},
    {1, 0x78},
    {1, 0x2C},
    {1, 0x2E},
    {1, 0x40},
    {1, 0x44},
    {2, 0xA6}, // TODO: Find out if 0xA6 or 0xD6 is correct
    {2, 0xD0},
    {2, 0xD2},
    {2, 0xA4},
    {2, 0x9A},
    {2, 0xA0},
    {1, 0xEE},
    {0, 0x40},
    {2, 0x54}
};



static void i2cWaitBusy(I2cRegs *const regs)
{
    while(regs->REG_I2C_CNT & I2C_ENABLE);
}

static I2cRegs* i2cGetBusRegsBase(u8 busId)
{
    I2cRegs *base;
    switch(busId)
    {
        case 0:
            base = (I2cRegs*)I2C1_REGS_BASE;
            break;
        case 1:
            base = (I2cRegs*)I2C2_REGS_BASE;
            break;
        case 2:
            base = (I2cRegs*)I2C3_REGS_BASE;
            break;
        default:
            base = NULL;
    }

    return base;
}

void I2C_init(void)
{
    I2cRegs *regs = i2cGetBusRegsBase(0); // Bus 1
    i2cWaitBusy(regs);
    regs->REG_I2C_CNTEX = 2;  // ?
    regs->REG_I2C_SCL = 1280; // ?

    regs = i2cGetBusRegsBase(1); // Bus 2
    i2cWaitBusy(regs);
    regs->REG_I2C_CNTEX = 2;  // ?
    regs->REG_I2C_SCL = 1280; // ?

    regs = i2cGetBusRegsBase(2); // Bus 3
    i2cWaitBusy(regs);
    regs->REG_I2C_CNTEX = 2;  // ?
    regs->REG_I2C_SCL = 1280; // ?
}

static bool i2cStartTransfer(I2cDevice devId, u8 regAddr, bool read, I2cRegs *const regs)
{
    const u8 devAddr = i2cDevTable[devId].devAddr;


    u32 i = 0;
    for(; i < 8; i++)
    {
        i2cWaitBusy(regs);

        // Select device and start.
        regs->REG_I2C_DATA = devAddr;
        regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_START;
        i2cWaitBusy(regs);
        if(!I2C_GET_ACK(regs->REG_I2C_CNT)) // If ack flag is 0 it failed.
        {
            regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
            continue;
        }

        // Select register and change direction to write.
        regs->REG_I2C_DATA = regAddr;
        regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_WRITE;
        i2cWaitBusy(regs);
        if(!I2C_GET_ACK(regs->REG_I2C_CNT)) // If ack flag is 0 it failed.
        {
            regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
            continue;
        }

        // Select device in read mode for read transfer.
        if(read)
        {
            regs->REG_I2C_DATA = devAddr | 1u; // Set bit 0 for read.
            regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_START;
            i2cWaitBusy(regs);
            if(!I2C_GET_ACK(regs->REG_I2C_CNT)) // If ack flag is 0 it failed.
            {
                regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
                continue;
            }
        }

        break;
    }

    if(i < 8) return true;
    else return false;
}

bool I2C_readRegBuf(I2cDevice devId, u8 regAddr, u8 *out, u32 size)
{
    const u8 busId = i2cDevTable[devId].busId;
    I2cRegs *const regs = i2cGetBusRegsBase(busId);


    if(!i2cStartTransfer(devId, regAddr, true, regs)) return false;

    while(--size)
    {
        regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_READ | I2C_ACK;
        i2cWaitBusy(regs);
        *out++ = regs->REG_I2C_DATA;
    }

    regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_READ | I2C_STOP;
    i2cWaitBusy(regs);
    *out = regs->REG_I2C_DATA; // Last byte

    return true;
}

bool I2C_writeRegBuf(I2cDevice devId, u8 regAddr, const u8 *in, u32 size)
{
    const u8 busId = i2cDevTable[devId].busId;
    I2cRegs *const regs = i2cGetBusRegsBase(busId);


    if(!i2cStartTransfer(devId, regAddr, false, regs)) return false;

    while(--size)
    {
        regs->REG_I2C_DATA = *in++;
        regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_WRITE;
        i2cWaitBusy(regs);
        if(!I2C_GET_ACK(regs->REG_I2C_CNT)) // If ack flag is 0 it failed.
        {
            regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
            return false;
        }
    }

    regs->REG_I2C_DATA = *in;
    regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_DIRE_WRITE | I2C_STOP;
    i2cWaitBusy(regs);
    if(!I2C_GET_ACK(regs->REG_I2C_CNT)) // If ack flag is 0 it failed.
    {
        regs->REG_I2C_CNT = I2C_ENABLE | I2C_IRQ_ENABLE | I2C_ERROR | I2C_STOP;
        return false;
    }

    return true;
}

u8 I2C_readReg(I2cDevice devId, u8 regAddr)
{
    u8 data;
    if(!I2C_readRegBuf(devId, regAddr, &data, 1)) return 0xFF;
    return data;
}

bool I2C_writeReg(I2cDevice devId, u8 regAddr, u8 data)
{
    return I2C_writeRegBuf(devId, regAddr, &data, 1);
}
