#include "appcompat.h"

compat_app_s* app = (compat_app_s *) APP_COMPAT;

#if defined(ENTRY_MSET)
compat_app_s mset_4x =
{
    .memcpy = (void*) 0x001BFA60,
    .GSPGPU_FlushDataCache = (void*) 0x001346C4,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x001AC924,
    .svcControlMemory = (void*) 0x001C3E24,
    .fopen = (void*) 0x001B82A8,
    .fread = (void*) 0x001B3954,
    .fwrite = (void*) 0x001B3B50,

    .gpuHandle = (0x0027C580 + 0x58),
};

compat_app_s mset_6x =
{
    .memcpy = (void*) 0x001C814C,
    .GSPGPU_FlushDataCache = (void*) 0x00134A84,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x001B4E8C,
    .svcControlMemory = (void*) 0x001CC63C,
    .fopen = (void*) 0x001C08B4,
    .fread = (void*) 0x001BC188,
    .fwrite = (void*) 0x001BC380,

    .gpuHandle = (0x0028A580 + 0x58),
};

#elif defined(ENTRY_SPIDER)
compat_app_s spider_4x =
{
    .memcpy = (void*) 0x0029BF60,
    .GSPGPU_FlushDataCache = (void*) 0x00344B84,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x002CF3EC,
    .svcControlMemory = (void*) 0x002D6ADC,
    .fopen = (void*) 0x0025B0A4,
    .fread = (void*) 0x002FC8E4,
    .fwrite = (void*) 0x00311D90,

    .GX_SetTextureCopy = (void*) 0x002C62E4,
    .svcSleepThread = (void*) 0x002A513C,

    .gpuHandle = (0x003F54E8 + 0x58),
};

compat_app_s spider_5x =
{
    .memcpy = (void*) 0x00240B58,
    .GSPGPU_FlushDataCache = (void*) 0x001914FC,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BF4C,
    .svcControlMemory = (void*) 0x001431C0,
    .fopen = (void*) 0x0022FE44,
    .fread = (void*) 0x001686C0,
    .fwrite = (void*) 0x00168748,

    .GX_SetTextureCopy = (void*) 0x0011DD80,
    .svcSleepThread = (void*) 0x0010420C,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_9x =
{
    .memcpy = (void*) 0x00240B50,
    .GSPGPU_FlushDataCache = (void*) 0x00191504,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BF04,
    .svcControlMemory = (void*) 0x001431A0,
    .fopen = (void*) 0x0022FE08,
    .fread = (void*) 0x001686DC,
    .fwrite = (void*) 0x00168764,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x0023FFE8,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_42_cn =
{
    .memcpy = (void*) 0x0023F048,
    .GSPGPU_FlushDataCache = (void*) 0x00190118,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA40,
    .svcControlMemory = (void*) 0x00142F64,
    .fopen = (void*) 0x0022E334,
    .fread = (void*) 0x001674BC,
    .fwrite = (void*) 0x00167544,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x00104218,

    .gpuHandle = (0x003D6C40 + 0x58),
};

compat_app_s spider_45_cn =
{
    .memcpy = (void*) 0x0023EFA0,
    .GSPGPU_FlushDataCache = (void*) 0x0018FC0C,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA54,
    .svcControlMemory = (void*) 0x00142F58,
    .fopen = (void*) 0x0022E2B0,
    .fread = (void*) 0x00166FC8,
    .fwrite = (void*) 0x00167050,

    .GX_SetTextureCopy = (void*) 0x0011DD68,
    .svcSleepThread = (void*) 0x0010420C,

    .gpuHandle = (0x003D6C40 + 0x58),
};

compat_app_s spider_5x_cn =
{
    .memcpy = (void*) 0x0023F80C,
    .GSPGPU_FlushDataCache = (void*) 0x001902A8,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA6C,
    .svcControlMemory = (void*) 0x00143110,
    .fopen = (void*) 0x0022EA5C,
    .fread = (void*) 0x0016751C,
    .fwrite = (void*) 0x001675A4,

    .GX_SetTextureCopy = (void*) 0x0011DD80,
    .svcSleepThread = (void*) 0x0010420C,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_9x_cn =
{
    .memcpy = (void*) 0x0023F808,
    .GSPGPU_FlushDataCache = (void*) 0x001902B8,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA24,
    .svcControlMemory = (void*) 0x001430F0,
    .fopen = (void*) 0x0022EA24,
    .fread = (void*) 0x00167540,
    .fwrite = (void*) 0x001675C8,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x001041F8,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_4x_kr =
{
    .memcpy = (void*) 0x0023FF90,
    .GSPGPU_FlushDataCache = (void*) 0x00190D30,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA14,
    .svcControlMemory = (void*) 0x00142FA0,
    .fopen = (void*) 0x0022F284,
    .fread = (void*) 0x001680F8,
    .fwrite = (void*) 0x00168180,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x00104218,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_5x_kr =
{
    .memcpy = (void*) 0x002407DC,
    .GSPGPU_FlushDataCache = (void*) 0x0019154C,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA40,
    .svcControlMemory = (void*) 0x00143108,
    .fopen = (void*) 0x0022FAC8,
    .fread = (void*) 0x001686FC,
    .fwrite = (void*) 0x00168784,

    .GX_SetTextureCopy = (void*) 0x0011DD80,
    .svcSleepThread = (void*) 0x0010420C,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_9x_kr =
{
    .memcpy = (void*) 0x002407D4,
    .GSPGPU_FlushDataCache = (void*) 0x00191554,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012B9F8,
    .svcControlMemory = (void*) 0x001430E8,
    .fopen = (void*) 0x0022FA8C,
    .fread = (void*) 0x00168718,
    .fwrite = (void*) 0x001687A0,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x001041F8,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_4x_tw =
{
    .memcpy = (void*) 0x0023FFE4,
    .GSPGPU_FlushDataCache = (void*) 0x00190D34,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA40,
    .svcControlMemory = (void*) 0x00142F64,
    .fopen = (void*) 0x0022F2D8,
    .fread = (void*) 0x001680FC,
    .fwrite = (void*) 0x00168184,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x00104218,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_5x_tw =
{
    .memcpy = (void*) 0x00240870,
    .GSPGPU_FlushDataCache = (void*) 0x00191594,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA6C,
    .svcControlMemory = (void*) 0x00143110,
    .fopen = (void*) 0x0022FB5C,
    .fread = (void*) 0x00168744,
    .fwrite = (void*) 0x001687CC,

    .GX_SetTextureCopy = (void*) 0x0011DD80,
    .svcSleepThread = (void*) 0x0010420C,

    .gpuHandle = (0x003D7C40 + 0x58),
};

compat_app_s spider_9x_tw =
{
    .memcpy = (void*) 0x00240868,
    .GSPGPU_FlushDataCache = (void*) 0x0019159C,
    .nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue = (void*) 0x0012BA24,
    .svcControlMemory = (void*) 0x001430F0,
    .fopen = (void*) 0x0022FB20,
    .fread = (void*) 0x00168760,
    .fwrite = (void*) 0x001687E8,

    .GX_SetTextureCopy = (void*) 0x0011DD48,
    .svcSleepThread = (void*) 0x001041F8,

    .gpuHandle = (0x003D7C40 + 0x58),
};

#endif

// Slow ass but safe memcpy
void _memcpy(void* dst, const void* src, uint32_t size)
{
    char *destc = (char *) dst;
    const char *srcc = (const char *) src;
    for(uint32_t i = 0; i < size; i++)
    {
        destc[i] = srcc[i];
    }
}

int set_app_offsets()
{
    const compat_app_s *sapp = 0;
    uint32_t app_spec = *(uint32_t *)0x0010000C;
    switch(app_spec)
    {
#if defined(ENTRY_MSET)
    case 0xEB0312BF:
        sapp = &mset_4x;
        break;
    case 0xEB0334CF:
        sapp = &mset_6x;
        break;
#elif defined(ENTRY_SPIDER)
    case 0xEB0676B5:
        sapp = &spider_4x;
        break;
    case 0xEB050B2A:
        sapp = &spider_5x;
        break;
    case 0xEB050B28:
        sapp = &spider_9x;
        break;
    case 0xEB050466:
        sapp = &spider_42_cn;
        break;
    case 0xEB05043C:
        sapp = &spider_45_cn;
        break;
    case 0xEB050657:
        sapp = &spider_5x_cn;
        break;
    case 0xEB050656:
        sapp = &spider_9x_cn;
        break;
    case 0xEB050838:
        sapp = &spider_4x_kr;
        break;
    case 0xEB050A4B:
        sapp = &spider_5x_kr;
        break;
    case 0xEB050A49:
        sapp = &spider_9x_kr;
        break;
    case 0xEB05084D:
        sapp = &spider_4x_tw;
        break;
    case 0xEB050A70:
        sapp = &spider_5x_tw;
        break;
    case 0xEB050A6E:
        sapp = &spider_9x_tw;
        break;
#endif
    }

    if (sapp) {
        _memcpy(app, sapp, sizeof(compat_app_s));
        return 0;
    }
    else
        return 1;
}
