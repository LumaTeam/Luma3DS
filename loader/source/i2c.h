#pragma once

#include "common.h"

#define I2C1_REG_OFF 0x10161000
#define I2C2_REG_OFF 0x10144000
#define I2C3_REG_OFF 0x10148000

#define I2C_REG_DATA  0
#define I2C_REG_CNT   1
#define I2C_REG_CNTEX 2
#define I2C_REG_SCL   4

#define I2C_DEV_MCU  3
#define I2C_DEV_GYRO 10
#define I2C_DEV_IR   13

u8 i2cGetDeviceBusId(u8 device_id);
u8 i2cGetDeviceRegAddr(u8 device_id);

vu8* i2cGetDataReg(u8 bus_id);
vu8* i2cGetCntReg(u8 bus_id);

void i2cWaitBusy(u8 bus_id);
bool i2cGetResult(u8 bus_id);
u8 i2cGetData(u8 bus_id);
void i2cStop(u8 bus_id, u8 arg0);

bool i2cSelectDevice(u8 bus_id, u8 dev_reg);
bool i2cSelectRegister(u8 bus_id, u8 reg);

u8 i2cReadRegister(u8 dev_id, u8 reg);
bool i2cWriteRegister(u8 dev_id, u8 reg, u8 data);

bool i2cReadRegisterBuffer(unsigned int dev_id, int reg, u8* buffer, size_t buf_size);
