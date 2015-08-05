#include "firmlaunchax.h"

#include <stdint.h>
#include "firmcompat.h"
#include "appcompat.h"
#include "arm11_tools.h"
#include "jump_table.h"

void setup_gpu()
{
    volatile uint32_t *top_left1 = (volatile uint32_t *)(fw->gpu_regs + 0x468);
    volatile uint32_t *top_left2 = (volatile uint32_t *)(fw->gpu_regs + 0x46C);
    volatile uint32_t *top_right1 = (volatile uint32_t *)(fw->gpu_regs + 0x494);
    volatile uint32_t *top_right2 = (volatile uint32_t *)(fw->gpu_regs + 0x498);
    volatile uint32_t *top_selected = (volatile uint32_t *)(fw->gpu_regs + 0x478);
    volatile uint32_t *bottom1 = (volatile uint32_t *)(fw->gpu_regs + 0x568);
    volatile uint32_t *bottom2 = (volatile uint32_t *)(fw->gpu_regs + 0x56C);
    volatile uint32_t *bottom_selected = (volatile uint32_t *)(fw->gpu_regs + 0x578);

    uint32_t *save = (uint32_t *)(fw->fcram_address + 0x3FFFE00);
    if (*top_selected) {
        save[0] = *top_left2;
        save[1] = *top_right2;
    } else {
        save[0] = *top_left1;
        save[1] = *top_right1;
    }

    if (*bottom_selected) {
        save[2] = *bottom2;
    } else {
        save[2] = *bottom1;
    }
}

void firmlaunch_arm9hax()
{
    invalidate_data_cache();
    invalidate_instruction_cache();

    // Copy arm9 code
    uint32_t code_offset = 0x3F00000;
    asm_memcpy((void *)(fw->fcram_address + code_offset),
               (void *)(fw->fcram_address + APP_CFW_OFFSET), ARM9_PAYLOAD_MAXSIZE);

    // Prepare framebuffer info for arm9
    setup_gpu();

    // Copy the jump table
    asm_memcpy((void *)fw->jump_table_address, &jump_table, (&jump_table_end - &jump_table + 1) * 4);

    // Write firmware-specific offsets to the jump table
    *(uint32_t *)(fw->jump_table_address +
                 (&jt_return - &jump_table) * 4) = fw->func_patch_return;
    *(uint32_t *)(fw->jump_table_address +
                 (&jt_pdn_regs - &jump_table) * 4) = fw->pdn_regs;
    *(uint32_t *)(fw->jump_table_address +
                 (&jt_pxi_regs - &jump_table) * 4) = fw->pxi_regs;

    // Patch arm11 functions
    *(uint32_t *)fw->func_patch_address = 0xE51FF004;
    *(uint32_t *)(fw->func_patch_address + 4) = 0xFFFF0C80;
    *(uint32_t *)fw->reboot_patch_address = 0xE51FF004;
    *(uint32_t *)(fw->reboot_patch_address + 4) = 0x1FFF4C80+4;

    invalidate_data_cache();

    // Trigger reboot
    ((void (*)())fw->reboot_func_address)(0, 0, 2, 0);

    while (1) {};
}
