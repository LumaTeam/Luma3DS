/*
srv.h

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "srv.h"

#include "services.h"
#include "notifications.h"
#include "processes.h"

Result srvHandleCommands(SessionData *sessionData)
{
    Result res = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    switch(cmdbuf[0] >> 16)
    {
        case 1: // RegisterClient
        {
            if(cmdbuf[0] == IPC_MakeHeader(1, 0, 2) && cmdbuf[1] == IPC_Desc_CurProcessId())
            {
                sessionData->pid = cmdbuf[2];
                cmdbuf[0] = IPC_MakeHeader(1, 1, 0);
                cmdbuf[1] = 0;
            }
            else
                goto invalid_command;
            break;
        }

        case 2: // EnableNotification
        {
            Handle notificationSemaphore = 0;
            res = EnableNotification(sessionData, &notificationSemaphore);
            cmdbuf[0] = IPC_MakeHeader(2, 1, 2);
            cmdbuf[1] = (u32)res;
            cmdbuf[2] = IPC_Desc_SharedHandles(1);
            cmdbuf[3] = notificationSemaphore;
            break;
        }

        case 3: // RegisterService
        {
            Handle serverPort = 0;
            res = RegisterService(sessionData, &serverPort, (const char *)(cmdbuf + 1), (s32)cmdbuf[3], (s32)cmdbuf[4]);
            cmdbuf[0] = IPC_MakeHeader(3, 1, 2);
            cmdbuf[1] = (u32)res;
            cmdbuf[2] = IPC_Desc_MoveHandles(1);
            cmdbuf[3] = serverPort;
            break;
        }

        case 4: // UnregisterService
        {
            res = UnregisterService(sessionData, (const char *)(cmdbuf + 1), (s32)cmdbuf[3]);
            cmdbuf[0] = IPC_MakeHeader(4, 1, 0);
            cmdbuf[1] = (u32)res;
            break;
        }

        case 5: // GetServiceHandle
        {
            Handle session = 0;
            res = GetServiceHandle(sessionData, &session, (const char *)(cmdbuf + 1), (s32)cmdbuf[3], cmdbuf[4]);
            if(R_MODULE(res) == RM_SRV && R_SUMMARY(res) == RS_WOULDBLOCK)
                memcpy(sessionData->replayCmdbuf, cmdbuf, 16);
            else
            {
                cmdbuf[0] = IPC_MakeHeader(5, 1, 2);
                cmdbuf[1] = (u32)res;
                cmdbuf[2] = IPC_Desc_MoveHandles(1);
                cmdbuf[3] = session;
            }
            break;
        }

        case 6: // RegisterPort
        {
            if(cmdbuf[0] == IPC_MakeHeader(6, 3, 2) && cmdbuf[4] == IPC_Desc_SharedHandles(1))
            {
                res = RegisterPort(sessionData, (Handle)cmdbuf[5], (const char *)(cmdbuf + 1), (s32)cmdbuf[3]);
                cmdbuf[0] = IPC_MakeHeader(6, 1, 0);
                cmdbuf[1] = (u32)res;
            }
            else
                goto invalid_command;
            break;
        }

        case 7: // UnregisterPort
        {
            res = UnregisterPort(sessionData, (const char *)(cmdbuf + 1), (s32)cmdbuf[3]);
            cmdbuf[0] = IPC_MakeHeader(7, 1, 0);
            cmdbuf[1] = (u32)res;
            break;
        }

        case 8: // GetPort
        {
            Handle port = 0;
            res = GetPort(sessionData, &port, (const char *)(cmdbuf + 1), (s32)cmdbuf[3], (u8)cmdbuf[4]);
            if(R_MODULE(res) == RM_SRV && R_SUMMARY(res) == RS_WOULDBLOCK)
                memcpy(sessionData->replayCmdbuf, cmdbuf, 16);
            else
            {
                cmdbuf[0] = IPC_MakeHeader(8, 1, 2);
                cmdbuf[1] = (u32)res;
                cmdbuf[2] = IPC_Desc_SharedHandles(1);
                cmdbuf[3] = port;
            }
            break;
        }

        case 9: // Subscribe
        {
            res = Subscribe(sessionData, cmdbuf[1]);
            cmdbuf[0] = IPC_MakeHeader(9, 1, 0);
            cmdbuf[1] = (u32)res;
            break;
        }

        case 10: // Unsubscribe
        {
            res = Unsubscribe(sessionData, cmdbuf[1]);
            cmdbuf[0] = IPC_MakeHeader(10, 1, 0);
            cmdbuf[1] = (u32)res;
            break;
        }

        case 11: // ReceiveNotification
        {
            u32 notificationId;
            res = ReceiveNotification(sessionData, &notificationId);
            cmdbuf[0] = IPC_MakeHeader(11, 2, 0);;
            cmdbuf[1] = (u32)res;
            cmdbuf[2] = notificationId;
            break;
        }

        case 12: // PublishToSubscriber
        {
            res = PublishToSubscriber(cmdbuf[1], cmdbuf[2]);
            cmdbuf[0] = IPC_MakeHeader(12, 1, 0);;
            cmdbuf[1] = (u32)res;
            break;
        }

        case 13: // PublishAndGetSubscriber
        {
            u32 pidCount;
            res = PublishAndGetSubscriber(&pidCount, cmdbuf + 3, cmdbuf[1], 0);
            cmdbuf[0] = IPC_MakeHeader(13, 62, 0);
            cmdbuf[1] = (u32)res;
            cmdbuf[2] = pidCount;
            break;
        }

        case 14: // IsServiceRegistered
        {
            bool isRegistered;
            res = IsServiceRegistered(sessionData, &isRegistered, (const char *)(cmdbuf + 1), (s32)cmdbuf[3]);
            cmdbuf[0] = IPC_MakeHeader(14, 2, 0);;
            cmdbuf[1] = (u32)res;
            cmdbuf[2] = isRegistered ? 1 : 0;
            break;
        }

        default:
            goto invalid_command;
            break;
    }

    return res;

invalid_command:
    cmdbuf[0] = IPC_MakeHeader(0, 1, 0);;
    cmdbuf[1] = (u32)0xD9001830;
    return 0xD9001830;
}
