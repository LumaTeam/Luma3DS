#include <3ds.h>

Handle mcuhwcHandle;

Result mcuInit(void);
Result mcuExit(void);
Result mcuReadRegister(u8 reg, u8* data, u32 size);
Result mcuWriteRegister(u8 reg, u8* data, u32 size);
Result mcuGetLEDState(u8* out);
