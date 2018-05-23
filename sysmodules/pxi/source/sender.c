/*
sender.c
    Handles commands from arm11 processes, then sends them to Process9, and replies to arm11 processes the replies received from Process9 (=> receiver.c).
    (except for PXISRV11)

(c) TuxSH, 2016-2017
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#include <string.h>

#include "sender.h"
#include "PXI.h"

Result sendPXICmdbuf(Handle *additionalHandle, u32 serviceId, u32 *buffer)
{

    Result res = 0;

    if(additionalHandle != NULL)
    {
        s32 index = 1;
        bool cancelled = false;
        Handle handles[2] = {PXITransferMutex, *additionalHandle};

        res = assertSuccess(svcWaitSynchronization(PXITransferMutex, 0LL));
        if(R_DESCRIPTION(res) == RD_TIMEOUT)
        {
            assertSuccess(svcWaitSynchronizationN(&index, handles, 2, false, -1LL));
            cancelled = index == 1;
        }
        else
            cancelled = false;

        if(cancelled)
            return 0xD92043FB; // cancel requested
    }
    else
        assertSuccess(svcWaitSynchronization(PXITransferMutex, -1LL));

    PXISendWord(serviceId & 0xFF);
    PXITriggerSync9IRQ(); //notify arm9
    PXISendBuffer(buffer, (buffer[0] & 0x3F) + ((buffer[0] & 0xFC0) >> 6) + 1);

    svcReleaseMutex(PXITransferMutex);
    return 0;
}

static void updateTLSForStaticBuffers(void)
{
    u32 *staticBufs = getThreadStaticBuffers();
    u32 val = sessionManager.currentlyProvidedStaticBuffers;
    for(u32 i = 0; i < 4; i++)
    {
        s32 pos = getLSBPosition(val);
        if(pos != -1)
        {
            staticBufs[2 * i] = IPC_Desc_StaticBuffer(0x1000, 0);
            staticBufs[2 * i + 1] = (u32)&staticBuffers[pos];
            val &= ~(1 << pos);
        }
        else
        {
            staticBufs[2 * i] = IPC_Desc_StaticBuffer(0, 0);
            staticBufs[2 * i + 1] = 0;
        }
    }
}

static void acquireStaticBuffers(void)
{
    u32 freeStaticBuffersOrig = sessionManager.freeStaticBuffers;
    sessionManager.freeStaticBuffers = clearMSBs(sessionManager.freeStaticBuffers, 4);
    sessionManager.currentlyProvidedStaticBuffers = ~sessionManager.freeStaticBuffers & freeStaticBuffersOrig;
    updateTLSForStaticBuffers();
}

static void releaseStaticBuffers(u32 *src, u32 nb)
{
    u32 val = clearMSBs(*src, nb);
    sessionManager.freeStaticBuffers |= ~val & *src;
    *src = val;
}

void sender(void)
{
    Handle handles[12] = {terminationRequestedEvent, sessionManager.sendAllBuffersToArm9Event, sessionManager.replySemaphore};
    Handle replyTarget = 0;
    Result res = 0;
    s32 index;

    u32 *cmdbuf = getThreadCommandBuffer();

    u32 nbIdleSessions = 0;
    u32 posToServiceId[9] = {0};
    RecursiveLock_Lock(&sessionManager.senderLock);

    //Setting static buffers is needed for IPC translation types 2 and 3 (otherwise ReplyAndReceive will dereference NULL)
    sessionManager.freeStaticBuffers = (1 << NB_STATIC_BUFFERS) - 1;
    acquireStaticBuffers();

    do
    {
        if(replyTarget == 0) //send to arm9
        {
            for(u32 i = 0; i < 9; i++)
            {
                SessionData *data = &sessionManager.sessionData[i];
                if(data->handle == 0 || data->state != STATE_RECEIVED_FROM_ARM11)
                    continue;

                if(sessionManager.sendingDisabled)
                {
                    if (sessionManager.pendingArm9Commands != 0 || sessionManager.latest_PXI_MC5_val == 0)
                        continue;
                }

                else
                    sessionManager.pendingArm9Commands++;

                RecursiveLock_Lock(&data->lock);
                data->state = STATE_SENT_TO_ARM9;
                res = sendPXICmdbuf(&terminationRequestedEvent, i, data->buffer);
                RecursiveLock_Unlock(&data->lock);

                if(R_FAILED(res))
                    goto terminate;

            }
            cmdbuf[0] = 0xFFFF0000; //Kernel11
        }

        nbIdleSessions = 0;
        for(u32 i = 0; i < 9; i++)
        {
            if(sessionManager.sessionData[i].handle != 0 && sessionManager.sessionData[i].state == STATE_IDLE)
            {
                handles[3 + nbIdleSessions] = sessionManager.sessionData[i].handle;
                posToServiceId[nbIdleSessions++] = i;
            }
        }

        RecursiveLock_Unlock(&sessionManager.senderLock);
        res = svcReplyAndReceive(&index, handles, 3 + nbIdleSessions, replyTarget);
        RecursiveLock_Lock(&sessionManager.senderLock);

        if((u32)res == 0xC920181A) //session closed by remote
        {
            u32 i;

            if(index == -1)
                for(i = 0; i < 9 && replyTarget != sessionManager.sessionData[i].handle; i++);

            else
                i = posToServiceId[index - 3];

            if(i >= 9) svcBreak(USERBREAK_PANIC);

            svcCloseHandle(sessionManager.sessionData[i].handle);
            sessionManager.sessionData[i].handle = replyTarget = 0;
            continue;
        }

        else if(R_FAILED(res) || (u32)index >= 3 + nbIdleSessions)
            svcBreak(USERBREAK_PANIC);

        switch(index)
        {
            case 0: //terminaton requested
                break;

            case 1:
                replyTarget = 0;
                continue;

            case 2: //arm9 reply
            {
                u32 sessionId = 0;
                for(sessionId = 0; sessionId < 9 && sessionManager.sessionData[sessionId].state != STATE_RECEIVED_FROM_ARM9; sessionId++);
                if(sessionId == 9) svcBreak(USERBREAK_PANIC);
                SessionData *data = &sessionManager.sessionData[sessionId];

                RecursiveLock_Lock(&data->lock);
                if(data->state != STATE_RECEIVED_FROM_ARM9) svcBreak(USERBREAK_PANIC);
                if(sessionManager.latest_PXI_MC5_val == 2)
                {
                    if(sessionManager.pendingArm9Commands != 0) svcBreak(USERBREAK_PANIC);
                    sessionManager.sendingDisabled = false;
                }
                else if(sessionManager.latest_PXI_MC5_val == 0)
                    (sessionManager.pendingArm9Commands)--;

                u32 bufSize = 4 * ((data->buffer[0] & 0x3F) + ((data->buffer[0] & 0xFC0) >> 6) + 1);
                if(bufSize > 0x100) svcBreak(USERBREAK_PANIC);
                memcpy(cmdbuf, data->buffer, bufSize);

                releaseStaticBuffers(&data->usedStaticBuffers, 4);

                data->state = STATE_IDLE;
                replyTarget = data->handle;

                RecursiveLock_Unlock(&data->lock);
                break;
            }

            default: //arm11 command received
            {
                u32 serviceId = posToServiceId[index - 3];
                SessionData *data = &sessionManager.sessionData[serviceId];
                RecursiveLock_Lock(&data->lock);

                if(data->state != STATE_IDLE) svcBreak(USERBREAK_PANIC);

                if(!(serviceId == 0 && (cmdbuf[0] >> 16) == 5)) //if not pxi:mc 5
                    sessionManager.latest_PXI_MC5_val = 0;
                else if((u8)(cmdbuf[1]) != 0)
                {
                    sessionManager.latest_PXI_MC5_val = 1;
                    if(sessionManager.sendingDisabled) svcBreak(USERBREAK_PANIC);
                    sessionManager.sendingDisabled = true;
                }
                else
                {
                    sessionManager.latest_PXI_MC5_val = 2;
                    if(!sessionManager.sendingDisabled) svcBreak(USERBREAK_PANIC);
                }

                u32 bufSize = 4 * ((cmdbuf[0] & 0x3F) + ((cmdbuf[0] & 0xFC0) >> 6) + 1);
                if(bufSize > 0x100) svcBreak(USERBREAK_PANIC);
                memcpy(data->buffer, cmdbuf, bufSize);

                data->state = STATE_RECEIVED_FROM_ARM11;
                replyTarget = 0;

                releaseStaticBuffers(&sessionManager.currentlyProvidedStaticBuffers, 4 - nbStaticBuffersByService[serviceId]);
                data->usedStaticBuffers = sessionManager.currentlyProvidedStaticBuffers;
                acquireStaticBuffers();

                RecursiveLock_Unlock(&data->lock);
                break;
            }
        }
    }
    while(index != 0);

terminate:
    for(u32 i = 0; i < 9; i++)
    {
        if(sessionManager.sessionData[i].handle != 0)
            svcCloseHandle(sessionManager.sessionData[i].handle);
    }

    RecursiveLock_Unlock(&sessionManager.senderLock);
}

void PXISRV11Handler(void)
{
    // Assumption: only 1 request is sent to this service at a time
    Handle handles[] = {sessionManager.PXISRV11CommandReceivedEvent, terminationRequestedEvent};
    SessionData *data = &sessionManager.sessionData[9];

    data->state = STATE_SENT_TO_ARM9;
    assertSuccess(svcSignalEvent(sessionManager.PXISRV11ReplySentEvent));

    while(true)
    {
        s32 index;
        assertSuccess(svcWaitSynchronizationN(&index, handles, 2, false, -1LL));

        if(index == 1) return;
        else
        {
            RecursiveLock_Lock(&data->lock);

            if(data->state != STATE_RECEIVED_FROM_ARM9)
                svcBreak(USERBREAK_PANIC);

            data->state = STATE_IDLE;

            if(data->buffer[0] >> 16 != 1)
            {
                data->buffer[0] = 0x40;
                data->buffer[1] = 0xD900182F; //unimplemented/invalid command
            }
            else
            {
                data->buffer[0] = 0x10040;
                data->buffer[1] = srvPublishToSubscriber(data->buffer[1], 1);

                data->state = STATE_RECEIVED_FROM_ARM11;
                if(data->buffer[1] == 0xD8606408)
                    svcBreak(USERBREAK_PANIC);
            }

            assertSuccess(sendPXICmdbuf(&terminationRequestedEvent, 9, data->buffer));
            data->state = STATE_SENT_TO_ARM9;
            assertSuccess(svcSignalEvent(sessionManager.PXISRV11ReplySentEvent));
            RecursiveLock_Unlock(&data->lock);
        }
    }
}
