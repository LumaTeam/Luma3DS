#pragma once

#include "types.h"

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

u32 i2cWriteRegister(u8 dev_id, u8 reg, u8 data);