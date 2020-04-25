/*
srv_pm.c

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "srv_pm.h"
#include "srv.h"
#include "services.h"
#include "notifications.h"
#include "processes.h"

Result srvPmHandleCommands(SessionData *sessionData)
{
    Result res = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    u32 mask = (cmdbuf[0] & 0x04000000) >> 16;

    if(IS_PRE_7X && mask == 0)
        return srvHandleCommands(sessionData);
    else if(!IS_PRE_7X && mask != 0)
        goto invalid_command;

    switch((cmdbuf[0] >> 16) & ~mask)
    {
        case 1: // PublishToProcess
        {
            if(cmdbuf[0] == IPC_MakeHeader(mask | 1, 1, 2) && cmdbuf[2] == IPC_Desc_SharedHandles(1))
            {
                res = PublishToProcess((Handle)cmdbuf[3], cmdbuf[1]);
                cmdbuf[0] = IPC_MakeHeader(mask | 1, 1, 0);
                cmdbuf[1] = (u32)res;
            }
            else
                goto invalid_command;
            break;
        }

        case 2: // PublishToAll
        {
            res = PublishToAll(cmdbuf[1]);
            cmdbuf[0] = IPC_MakeHeader(mask | 2, 1, 0);
            cmdbuf[1] = (u32)res;
            break;
        }

        case 3: // RegisterProcess
        {
            if(cmdbuf[0] == IPC_MakeHeader(mask | 3, 2, 2) && (cmdbuf[3] & 0x3C0F) == (IPC_Desc_StaticBuffer(0x110, 0) & 0x3C0F))
            {
                res = RegisterProcess(cmdbuf[1], (char (*)[8])cmdbuf[4], cmdbuf[3] >> 14);
                cmdbuf[0] = IPC_MakeHeader(mask | 3, 1, 0);
                cmdbuf[1] = (u32)res;
            }
            else
                goto invalid_command;
            break;
        }

        case 4:
        {
            res = UnregisterProcess(cmdbuf[1]);
            cmdbuf[0] = IPC_MakeHeader(mask | 4, 1, 0);
            cmdbuf[1] = (u32)res;
            break;
        }

        default:
            goto invalid_command;
            break;
    }

    return res;

invalid_command:
    cmdbuf[0] = IPC_MakeHeader(0, 1, 0);;
    cmdbuf[1] = (u32)0xD900182F;
    return 0xD900182F;
}

