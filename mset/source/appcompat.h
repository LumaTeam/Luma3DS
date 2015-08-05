#ifndef __appcompat_h__
#define __appcompat_h__

#include <stdint.h>
#include "launcher_path.h"

typedef struct compat_app_s
{
    void (*memcpy)(void *dest, void *src, uint32_t len);
    int (*GSPGPU_FlushDataCache)(void *address, uint32_t length);
    void (*nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue)(void *arg1, void *arg2);
    uint32_t (*svcControlMemory)(uint32_t *outaddr, uint32_t *addr0, uint32_t *addr1, uint32_t size, uint32_t operation, uint32_t permissions);
    int (*fopen)(uint32_t (*handle)[], short unsigned int *path, int flags);
    int (*fread)(uint32_t (*handle)[], uint32_t *read, void *buffer, uint32_t size);
    int (*fwrite)(uint32_t (*handle)[], uint32_t *written, void *src, uint32_t size);

    uint32_t gpuHandle;

#if defined(ENTRY_SPIDER)
    int (*GX_SetTextureCopy)(void *input_buffer, void *output_buffer, uint32_t size, int in_x, int in_y, int out_x, int out_y, int flags);
    int (*svcSleepThread)(unsigned long long nanoseconds);
#endif
} compat_app_s;

extern compat_app_s* app;
int set_app_offsets();

#if defined(ENTRY_MSET)
    // The usable area for this app
    #define APP_FCRAM_ADDR 0x14000000

    #define APP_CFW_OFFSET 0x400000
    #define APP_LAUNCHER_PATH (L"YS:/" LAUNCHER_PATH)

#elif defined(ENTRY_SPIDER)
    // The usable area for this app
    #define APP_FCRAM_ADDR 0x18400000

    #define APP_CFW_OFFSET 0x4410000
    #define APP_LAUNCHER_PATH (L"dmc:/" LAUNCHER_PATH)
#endif

// Locations in fcram
#define APP_CHECK_MEM (APP_FCRAM_ADDR + 0x1000)
#define APP_ARM11_BUFFER (APP_FCRAM_ADDR + 0x2000)
#define APP_MEM_HAX_MEM (APP_FCRAM_ADDR + 0x50000)
#define APP_COMPAT (APP_FCRAM_ADDR + 0x20000)
#define APP_FIRM_COMPAT (APP_FCRAM_ADDR + 0x20100)

#define ARM9_PAYLOAD_MAXSIZE 0x10000

#endif
