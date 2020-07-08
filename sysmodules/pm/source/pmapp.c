#include <3ds.h>
#include <string.h>
#include "launch.h"
#include "firmlaunch.h"
#include "termination.h"
#include "info.h"
#include "reslimit.h"
#include "manager.h"
#include "util.h"

void pmAppHandleCommands(void *ctx)
{
    (void)ctx;
    u32 *cmdbuf = getThreadCommandBuffer();
    u32 cmdhdr = cmdbuf[0];

    FS_ProgramInfo programInfo, programInfoUpdate;
    ExHeader_Arm11CoreInfo coreInfo;
    ExHeader_SystemInfoFlags siFlags;
    u32 pid;
    u64 titleId, mbz;
    s64 timeout, limit;
    void *buf;
    size_t size;

    switch (cmdhdr >> 16) {
        case 1:
            memcpy(&programInfo, cmdbuf + 1, sizeof(FS_ProgramInfo));
            cmdbuf[1] = LaunchTitle(&pid, &programInfo, cmdbuf[5]);
            cmdbuf[2] = pid;
            cmdbuf[0] = IPC_MakeHeader(1, 2, 0);
            break;
        case 2:
            if (cmdhdr != IPC_MakeHeader(2, 2, 2) || (cmdbuf[3] & 0xF) != 0xA) {
                goto invalid_command;
            }
            size = cmdbuf[3] >> 4;
            buf = (void *)cmdbuf[4];
            cmdbuf[1] = LaunchFirm(cmdbuf[1], buf, size);
            cmdbuf[0] = IPC_MakeHeader(2, 1, 2);
            cmdbuf[2] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
            cmdbuf[3] = (u32)buf;
            break;
        case 3:
            memcpy(&timeout, cmdbuf + 1, 8);
            cmdbuf[1] = TerminateApplication(timeout);
            cmdbuf[0] = IPC_MakeHeader(3, 1, 0);
            break;
        case 4:
            memcpy(&titleId, cmdbuf + 1, 8);
            memcpy(&timeout, cmdbuf + 3, 8);
            cmdbuf[1] = TerminateTitle(titleId, timeout);
            cmdbuf[0] = IPC_MakeHeader(4, 1, 0);
            break;
        case 5:
            memcpy(&timeout, cmdbuf + 2, 8);
            cmdbuf[1] = TerminateProcess(cmdbuf[1], timeout);
            cmdbuf[0] = IPC_MakeHeader(5, 1, 0);
            break;
        case 6:
            if (cmdhdr != IPC_MakeHeader(6, 2, 2) || cmdbuf[3] != 0x20) {
                goto invalid_command;
            }
            memcpy(&timeout, cmdbuf + 1, 8);
            cmdbuf[1] = PrepareForReboot(cmdbuf[4], timeout);
            cmdbuf[0] = IPC_MakeHeader(6, 1, 0);
            break;
        case 7:
            if (cmdhdr != IPC_MakeHeader(7, 1, 2) || (cmdbuf[2] & 0xF) != 0xC) {
                goto invalid_command;
            }
            size = cmdbuf[2] >> 4;
            buf = (void *)cmdbuf[3];
            cmdbuf[1] = GetFirmlaunchParams(buf, size);
            cmdbuf[0] = IPC_MakeHeader(7, 1, 2);
            cmdbuf[2] = IPC_Desc_Buffer(size, IPC_BUFFER_W);
            cmdbuf[3] = (u32)buf;
            break;
        case 8:
            memcpy(&programInfo, cmdbuf + 1, sizeof(FS_ProgramInfo));
            cmdbuf[1] = GetTitleExHeaderFlags(&coreInfo, &siFlags, &programInfo);
            cmdbuf[0] = IPC_MakeHeader(8, 5, 0);
            memcpy(cmdbuf + 2, &coreInfo, sizeof(ExHeader_Arm11CoreInfo));
            memcpy(cmdbuf + 4, &siFlags, sizeof(ExHeader_SystemInfoFlags));
            break;
        case 9:
            if (cmdhdr != IPC_MakeHeader(9, 1, 2) || (cmdbuf[2] & 0xF) != 0xA) {
                goto invalid_command;
            }
            size = cmdbuf[2] >> 4;
            buf = (void *)cmdbuf[3];
            cmdbuf[1] = SetFirmlaunchParams(buf, size);
            cmdbuf[0] = IPC_MakeHeader(9, 1, 2);
            cmdbuf[2] = IPC_Desc_Buffer(size, IPC_BUFFER_R);
            cmdbuf[3] = (u32)buf;
            break;
        case 10:
            memcpy(&mbz, cmdbuf + 4, 8);
            cmdbuf[1] = SetAppResourceLimit(cmdbuf[1], (ResourceLimitType)cmdbuf[2], cmdbuf[3], mbz);
            cmdbuf[0] = IPC_MakeHeader(10, 1, 0);
            break;
        case 11:
            memcpy(&mbz, cmdbuf + 4, 8);
            cmdbuf[1] = GetAppResourceLimit(&limit, cmdbuf[1], (ResourceLimitType)cmdbuf[2], cmdbuf[3], mbz);
            cmdbuf[0] = IPC_MakeHeader(11, 3, 0);
            memcpy(cmdbuf + 2, &limit, 8);
            break;
        case 12:
            memcpy(&titleId, cmdbuf + 1, 8);
            cmdbuf[1] = UnregisterProcess(titleId);
            cmdbuf[0] = IPC_MakeHeader(12, 1, 0);
            break;
        case 13:
            memcpy(&programInfo, cmdbuf + 1, sizeof(FS_ProgramInfo));
            memcpy(&programInfoUpdate, cmdbuf + 5, sizeof(FS_ProgramInfo));
            cmdbuf[1] = LaunchTitleUpdate(&programInfo, &programInfoUpdate, cmdbuf[9]);
            break;
        default:
            cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
            cmdbuf[1] = 0xD900182F;
            break;
    }

    return;

    invalid_command:
    cmdbuf[0] = IPC_MakeHeader(0, 1, 0);
    cmdbuf[1] = 0xD9001830;
}
