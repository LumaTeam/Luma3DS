/*
*   This file is part of Luma3DS
*   Copyright (C) 2016 Aurora Wright, TuxSH
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
*   Additional Terms 7.b of GPLv3 applies to this file: Requiring preservation of specified
*   reasonable legal notices or author attributions in that material or in the Appropriate Legal
*   Notices displayed by works containing it.
*/

/*
*   Thanks to the everyone who contributed in the development of this file
*/

#include "i2c.h"

//-----------------------------------------------------------------------------

static const struct { u8 bus_id, reg_addr; } dev_data[] = {
    {0, 0x4A}, {0, 0x7A}, {0, 0x78},
    {1, 0x4A}, {1, 0x78}, {1, 0x2C},
    {1, 0x2E}, {1, 0x40}, {1, 0x44},
    {2, 0xD6}, {2, 0xD0}, {2, 0xD2},
    {2, 0xA4}, {2, 0x9A}, {2, 0xA0},
};

static inline u8 i2cGetDeviceBusId(u8 device_id)
{
    return dev_data[device_id].bus_id;
}

static inline u8 i2cGetDeviceRegAddr(u8 device_id)
{
    return dev_data[device_id].reg_addr;
}

//-----------------------------------------------------------------------------

static vu8 *reg_data_addrs[] = {
    (vu8 *)(I2C1_REG_OFF + I2C_REG_DATA),
    (vu8 *)(I2C2_REG_OFF + I2C_REG_DATA),
    (vu8 *)(I2C3_REG_OFF + I2C_REG_DATA),
};

static inline vu8 *i2cGetDataReg(u8 bus_id)
{
    return reg_data_addrs[bus_id];
}

//-----------------------------------------------------------------------------

static vu8 *reg_cnt_addrs[] = {
    (vu8 *)(I2C1_REG_OFF + I2C_REG_CNT),
    (vu8 *)(I2C2_REG_OFF + I2C_REG_CNT),
    (vu8 *)(I2C3_REG_OFF + I2C_REG_CNT),
};

static inline vu8 *i2cGetCntReg(u8 bus_id)
{
    return reg_cnt_addrs[bus_id];
}

//-----------------------------------------------------------------------------

static inline void i2cWaitBusy(u8 bus_id)
{
    while (*i2cGetCntReg(bus_id) & 0x80);
}

static inline bool i2cGetResult(u8 bus_id)
{
    i2cWaitBusy(bus_id);

    return (*i2cGetCntReg(bus_id) >> 4) & 1;
}

static void i2cStop(u8 bus_id, u8 arg0)
{
    *i2cGetCntReg(bus_id) = (arg0 << 5) | 0xC0;
    i2cWaitBusy(bus_id);
    *i2cGetCntReg(bus_id) = 0xC5;
}

//-----------------------------------------------------------------------------

static bool i2cSelectDevice(u8 bus_id, u8 dev_reg)
{
    i2cWaitBusy(bus_id);
    *i2cGetDataReg(bus_id) = dev_reg;
    *i2cGetCntReg(bus_id) = 0xC2;

    return i2cGetResult(bus_id);
}

static bool i2cSelectRegister(u8 bus_id, u8 reg)
{
    i2cWaitBusy(bus_id);
    *i2cGetDataReg(bus_id) = reg;
    *i2cGetCntReg(bus_id) = 0xC0;

    return i2cGetResult(bus_id);
}

//-----------------------------------------------------------------------------

bool i2cWriteRegister(u8 dev_id, u8 reg, u8 data)
{
    u8 bus_id = i2cGetDeviceBusId(dev_id),
       dev_addr = i2cGetDeviceRegAddr(dev_id);

    for(u32 i = 0; i < 8; i++)
    {
        if(i2cSelectDevice(bus_id, dev_addr) && i2cSelectRegister(bus_id, reg))
        {
            i2cWaitBusy(bus_id);
            *i2cGetDataReg(bus_id) = data;
            *i2cGetCntReg(bus_id) = 0xC1;
            i2cStop(bus_id, 0);

            if(i2cGetResult(bus_id)) return true;
        }
        *i2cGetCntReg(bus_id) = 0xC5;
        i2cWaitBusy(bus_id);
    }

    return false;
}