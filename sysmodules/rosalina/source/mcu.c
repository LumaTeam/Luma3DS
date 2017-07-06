#include "mcu.h"

Result mcuInit(void)
{
    return srvGetServiceHandle(&mcuhwcHandle, "mcu::HWC");
}

Result mcuExit(void)
{
    return svcCloseHandle(mcuhwcHandle);
}

Result mcuReadRegister(u8 reg, u8* data, u32 size)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x10082;
    ipc[1] = reg;
    ipc[2] = size;
    ipc[3] = size << 4 | 0xC;
    ipc[4] = (u32)data;
    Result ret = svcSendSyncRequest(mcuhwcHandle);
    if(ret < 0) return ret;
    return ipc[1];
}

Result mcuWriteRegister(u8 reg, u8* data, u32 size)
{
    u32* ipc = getThreadCommandBuffer();
    ipc[0] = 0x20082;
    ipc[1] = reg;
    ipc[2] = size;
    ipc[3] = size << 4 | 0xA;
    ipc[4] = (u32)data;
    Result ret = svcSendSyncRequest(mcuhwcHandle);
    if(ret < 0) return ret;
    return ipc[1];
}

Result mcuGetLEDState(u8* out)
{
  return mcuReadRegister(0x28, out, 1);
}
