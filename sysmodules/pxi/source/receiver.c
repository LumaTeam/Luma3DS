/*
receiver.c:
    Fetches replies coming from Process9, writing them in the appropriate buffer.

(c) TuxSH, 2016-2017
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#include <string.h>

#include "receiver.h"
#include "PXI.h"

static inline void receiveFromArm9(void)
{
    u32 serviceId = PXIReceiveWord();

    //The offcical implementation can return 0xD90043FA
    if(((serviceId >= 10)) || (sessionManager.sessionData[serviceId].state != STATE_SENT_TO_ARM9))
        svcBreak(USERBREAK_PANIC);

    sessionManager.receivedServiceId = serviceId;
    RecursiveLock_Lock(&sessionManager.sessionData[serviceId].lock);
    u32 replyHeader = PXIReceiveWord();
    u32 replySizeWords = (replyHeader & 0x3F) + ((replyHeader & 0xFC0) >> 6) + 1;

    if(replySizeWords > 0x40) svcBreak(USERBREAK_PANIC);

    u32 *buf = sessionManager.sessionData[serviceId].buffer;

    buf[0] = replyHeader;
    PXIReceiveBuffer(buf + 1, replySizeWords - 1);
    sessionManager.sessionData[serviceId].state = STATE_RECEIVED_FROM_ARM9;
    RecursiveLock_Unlock(&sessionManager.sessionData[serviceId].lock);

    if(serviceId == 0 && shouldTerminate)
    {
        assertSuccess(svcSignalEvent(terminationRequestedEvent));
        return;
    }

    if(serviceId != 9)
    {
        s32 count;
        assertSuccess(svcReleaseSemaphore(&count, sessionManager.replySemaphore, 1));
    }
    else
    {
        assertSuccess(svcSignalEvent(sessionManager.PXISRV11CommandReceivedEvent));
        assertSuccess(svcWaitSynchronization(sessionManager.PXISRV11ReplySentEvent, -1LL));
        if( (sessionManager.sessionData[serviceId].state != STATE_SENT_TO_ARM9))
            svcBreak(USERBREAK_PANIC);
    }
}

void receiver(void)
{
    Handle handles[] = {PXISyncInterrupt, terminationRequestedEvent};

    assertSuccess(svcWaitSynchronization(sessionManager.PXISRV11ReplySentEvent, -1LL));
    while(true)
    {
        s32 index;
        assertSuccess(svcWaitSynchronizationN(&index, handles, 2, false, -1LL));

        if(index == 1) return;
        while(!PXIIsReceiveFIFOEmpty())
            receiveFromArm9();
    }
}
