#include "i2c.h"

//-----------------------------------------------------------------------------

static const struct { u8 bus_id, reg_addr; } dev_data[] = {
    {0, 0x4A}, {0, 0x7A}, {0, 0x78},
    {1, 0x4A}, {1, 0x78}, {1, 0x2C},
    {1, 0x2E}, {1, 0x40}, {1, 0x44},
    {2, 0xD6}, {2, 0xD0}, {2, 0xD2},
    {2, 0xA4}, {2, 0x9A}, {2, 0xA0},
};

static inline u8 i2cGetDeviceBusId(u8 device_id) {
    return dev_data[device_id].bus_id;
}

static inline u8 i2cGetDeviceRegAddr(u8 device_id) {
    return dev_data[device_id].reg_addr;
}

//-----------------------------------------------------------------------------

static vu8* reg_data_addrs[] = {
    (vu8*)(I2C1_REG_OFF + I2C_REG_DATA),
    (vu8*)(I2C2_REG_OFF + I2C_REG_DATA),
    (vu8*)(I2C3_REG_OFF + I2C_REG_DATA),
};

static inline vu8* i2cGetDataReg(u8 bus_id) {
    return reg_data_addrs[bus_id];
}

//-----------------------------------------------------------------------------

static vu8* reg_cnt_addrs[] = {
    (vu8*)(I2C1_REG_OFF + I2C_REG_CNT),
    (vu8*)(I2C2_REG_OFF + I2C_REG_CNT),
    (vu8*)(I2C3_REG_OFF + I2C_REG_CNT),
};

static inline vu8* i2cGetCntReg(u8 bus_id) {
    return reg_cnt_addrs[bus_id];
}

//-----------------------------------------------------------------------------

static inline void i2cWaitBusy(u8 bus_id) {
    while (*i2cGetCntReg(bus_id) & 0x80);
}

static inline u32 i2cGetResult(u8 bus_id) {
    i2cWaitBusy(bus_id);
    return (*i2cGetCntReg(bus_id) >> 4) & 1;
}

static void i2cStop(u8 bus_id, u8 arg0) {
    *i2cGetCntReg(bus_id) = (arg0 << 5) | 0xC0;
    i2cWaitBusy(bus_id);
    *i2cGetCntReg(bus_id) = 0xC5;
}

//-----------------------------------------------------------------------------

static u32 i2cSelectDevice(u8 bus_id, u8 dev_reg) {
    i2cWaitBusy(bus_id);
    *i2cGetDataReg(bus_id) = dev_reg;
    *i2cGetCntReg(bus_id) = 0xC2;
    return i2cGetResult(bus_id);
}

static u32 i2cSelectRegister(u8 bus_id, u8 reg) {
    i2cWaitBusy(bus_id);
    *i2cGetDataReg(bus_id) = reg;
    *i2cGetCntReg(bus_id) = 0xC0;
    return i2cGetResult(bus_id);
}

//-----------------------------------------------------------------------------

u32 i2cWriteRegister(u8 dev_id, u8 reg, u8 data) {
    u8 bus_id = i2cGetDeviceBusId(dev_id);
    u8 dev_addr = i2cGetDeviceRegAddr(dev_id);

    for (int i = 0; i < 8; i++) {
        if (i2cSelectDevice(bus_id, dev_addr) && i2cSelectRegister(bus_id, reg)) {
            i2cWaitBusy(bus_id);
            *i2cGetDataReg(bus_id) = data;
            *i2cGetCntReg(bus_id) = 0xC1;
            i2cStop(bus_id, 0);
            if (i2cGetResult(bus_id))
                return 1;
        }
        *i2cGetCntReg(bus_id) = 0xC5;
        i2cWaitBusy(bus_id);
    }

    return 0;
}
