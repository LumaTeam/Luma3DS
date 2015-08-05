#include "firmcompat.h"

#include <stdint.h>

struct firmware_offsets *fw = (struct firmware_offsets *)APP_FIRM_COMPAT;

int set_firmware_offsets()
{
    uint32_t kernel_version = *(uint32_t *)0x1FF80000;

    // Offsets taken from bootstrap
    switch (kernel_version) {

        case 0x022E0000:  // N3DS 9.0.0
            fw->kernel_patch_address = 0xDFF8382F;
            fw->reboot_patch_address = 0xDFFF4994;
            fw->reboot_func_address = 0xFFF158F8;
            fw->jump_table_address = 0xDFFF4C80;
            fw->fcram_address = 0xE0000000;
            fw->func_patch_address = 0xDFFE7A50;
            fw->func_patch_return = 0xFFF28A58;
            fw->pdn_regs = 0xFFFBE000;
            fw->pxi_regs = 0xFFFC0000;
            fw->gpu_regs = 0xFFFBC000;
            break;
            
        case 0x022C0600:  // N3DS 8.0.0
            fw->kernel_patch_address = 0xDFF8382F;
            fw->reboot_patch_address = 0xDFFF4994;
            fw->reboot_func_address = 0xFFF158F8;
            fw->jump_table_address = 0xDFFF4C80;
            fw->fcram_address = 0xE0000000;
            fw->func_patch_address = 0xDFFE7A50;
            fw->func_patch_return = 0xFFF28A58;
            fw->pdn_regs = 0xFFFBE000;
            fw->pxi_regs = 0xFFFC0000;
            fw->gpu_regs = 0xFFFBC000;
            break;

        default:
            return 1;
    }

    return 0;
}
