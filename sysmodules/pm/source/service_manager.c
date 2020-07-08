/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include <3ds.h>
#include "service_manager.h"

#define TRY(expr) if(R_FAILED(res = (expr))) goto cleanup;

Result ServiceManager_Run(const ServiceManagerServiceEntry *services, const ServiceManagerNotificationEntry *notifications, const ServiceManagerContextAllocator *allocator)
{
    Result res = 0;

    u32 numServices = 0;
    u32 maxSessionsTotal = 0;
    u32 numActiveSessions = 0;
    bool terminationRequested = false;

    for (u32 i = 0; services[i].name != NULL; i++) {
        numServices++;
        maxSessionsTotal += services[i].maxSessions;
    }

    Handle waitHandles[1 + numServices + maxSessionsTotal];
    void *ctxs[maxSessionsTotal];
    u8 handlerIds[maxSessionsTotal];

    Handle replyTarget = 0;
    s32 id = -1;
    u32 *cmdbuf = getThreadCommandBuffer();

    TRY(srvEnableNotification(&waitHandles[0]));

    // Subscribe to notifications if needed.
    for (u32 i = 0; notifications[i].handler != NULL; i++) {
        // Termination & ready for reboot events send by PM using PublishToProcess don't require subscription.
        if (notifications[i].id != 0x100 && notifications[i].id != 0x179) {
            TRY(srvSubscribe(notifications[i].id));
        }
    }

    for (u32 i = 0; i < numServices; i++) {
        if (!services[i].isGlobalPort) {
            TRY(srvRegisterService(&waitHandles[1 + i], services[i].name, (s32)services[i].maxSessions));
        } else {
            Handle clientPort;
            TRY(svcCreatePort(&waitHandles[1 + i], &clientPort, services[i].name, (s32)services[i].maxSessions));
            svcCloseHandle(clientPort);
        }
    }

    while (!terminationRequested) {
        if (replyTarget == 0) {
            cmdbuf[0] = 0xFFFF0000;
        }

        id = -1;
        res = svcReplyAndReceive(&id, waitHandles, 1 + numServices + numActiveSessions, replyTarget);

        if (res == (Result)0xC920181A) {
            // Session has been closed
            u32 off;
            if (id == -1) {
                for (off = 0; off < numActiveSessions && waitHandles[1 + numServices + off] != replyTarget; off++);
                if (off >= numActiveSessions) {
                    return res;
                }

                id = 1 + numServices + off;
            } else if ((u32)id < 1 + numServices) {
                return res;
            }

            off = id - 1 - numServices;

            Handle h = waitHandles[id];
            void *ctx = ctxs[off];
            waitHandles[id] = waitHandles[1 + numServices + --numActiveSessions];
            handlerIds[off] = handlerIds[numActiveSessions];
            ctxs[off] = ctxs[numActiveSessions];

            svcCloseHandle(h);
            if (allocator != NULL) {
                allocator->freeSessionContext(ctx);
            }

            replyTarget = 0;
            res = 0;
        } else if (R_FAILED(res)) {
            return res;
        }

        else {
            // Ok, no session closed and no error
            replyTarget = 0;
            if (id == 0) {
                // Notification
                u32 notificationId = 0;
                TRY(srvReceiveNotification(&notificationId));
                terminationRequested = notificationId == 0x100;

                for (u32 i = 0; notifications[i].handler != NULL; i++) {
                    if (notifications[i].id == notificationId) {
                        notifications[i].handler(notificationId);
                        break;
                    }
                }
            } else if ((u32)id < 1 + numServices) {
                // New session
                Handle session;
                void *ctx = NULL;
                TRY(svcAcceptSession(&session, waitHandles[id]));

                if (allocator) {
                    ctx = allocator->newSessionContext((u8)(id - 1));
                    if (ctx == NULL) {
                        svcCloseHandle(session);
                        return 0xDEAD0000;
                    }
                }

                waitHandles[1 + numServices + numActiveSessions] = session;
                handlerIds[numActiveSessions] = (u8)(id - 1);
                ctxs[numActiveSessions++] = ctx;
            } else {
                // Service command
                u32 off = id - 1 - numServices;
                services[handlerIds[off]].handler(ctxs[off]);
                replyTarget = waitHandles[id];
            }
        }
    }

cleanup:
    for (u32 i = 0; i < 1 + numServices + numActiveSessions; i++) {
        svcCloseHandle(waitHandles[i]);
    }

    // Subscribe to notifications if needed.
    for (u32 i = 0; notifications[i].handler != NULL; i++) {
        // Termination & ready for reboot events send by PM using PublishToProcess don't require subscription.
        if (notifications[i].id != 0x100 && notifications[i].id != 0x179) {
            TRY(srvUnsubscribe(notifications[i].id));
        }
    }

    for (u32 i = 0; i < numServices; i++) {
        if (!services[i].isGlobalPort) {
            srvUnregisterService(services[i].name);
        }
    }

    if (allocator) {
        for (u32 i = 0; i < numActiveSessions; i++) {
            allocator->freeSessionContext(ctxs[i]);
        }
    }

    return res;
}
