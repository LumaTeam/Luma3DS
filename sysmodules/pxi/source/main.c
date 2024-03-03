/*
main.c
    (De)initialization stuff. It's also here where sessions are being accepted.

(c) TuxSH, 2016-2020
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#include <string.h>
#include "PXI.h"
#include "common.h"
#include "MyThread.h"
#include "receiver.h"
#include "sender.h"

Handle PXISyncInterrupt = 0, PXITransferMutex = 0;
Handle terminationRequestedEvent = 0;
bool shouldTerminate = false;
SessionManager sessionManager = {0};

const char *serviceNames[10] =
{
    "pxi:mc",

    "PxiFS0",
    "PxiFS1",
    "PxiFSB",
    "PxiFSR",

    "PxiPM",

    "pxi:dev", //in the official PXI module maxSessions(pxi:dev) == 2. It doesn't matter anyways, since srvSysRegisterService is always called with 1
    "pxi:am9",
    "pxi:ps9",

    "pxi:srv11",
};

const u32 nbStaticBuffersByService[10] = {0, 2, 2, 2, 2, 1, 4, 4, 4, 0};

u32 CTR_ALIGN(0x1000) staticBuffers[NB_STATIC_BUFFERS][0x400] = {{0}};

static inline void initPXI(void)
{
    Result res;

    Handle handles[2] = {0};

    PXIReset();

    if(PXISyncInterrupt != 0) svcBreak(USERBREAK_PANIC); //0xE0A0183B
    assertSuccess(svcCreateEvent(&PXISyncInterrupt, RESET_ONESHOT));

    if(PXITransferMutex != 0) svcBreak(USERBREAK_PANIC); //0xE0A0183B
    assertSuccess(svcCreateMutex(&PXITransferMutex, false));

    assertSuccess(svcCreateEvent(&handles[0], RESET_ONESHOT)); //receive FIFO not empty
    assertSuccess(svcCreateEvent(&handles[1], RESET_ONESHOT)); //send FIFO empty
    assertSuccess(bindPXIInterrupts(&PXISyncInterrupt, &handles[0], &handles[1]));

    s32 handleIndex;
    do
    {
        while(!PXIIsSendFIFOFull()) PXISendWord(0);

        res = assertSuccess(svcWaitSynchronization(handles[0], 0LL));
        if(R_DESCRIPTION(res) == RD_TIMEOUT)
            assertSuccess(svcWaitSynchronizationN(&handleIndex, handles, 2, false, -1LL));
        else
            handleIndex = 0;
    } while(handleIndex != 0);



    unbindPXIInterrupts(NULL, &handles[0], &handles[1]);

    PXISendByte(1);
    while(PXIReceiveByte() < 1);

    while (!PXIIsReceiveFIFOEmpty())
        PXIReceiveWord();

    PXISendByte(2);
    while(PXIReceiveByte() < 2);

    svcCloseHandle(handles[0]);
    svcCloseHandle(handles[1]);
}

static inline void exitPXI(void)
{
    unbindPXIInterrupts(&PXISyncInterrupt, NULL, NULL);
    svcCloseHandle(PXITransferMutex);
    svcCloseHandle(PXISyncInterrupt);
    PXIReset();
}

static u8 CTR_ALIGN(8) receiverStack[THREAD_STACK_SIZE];
static u8 CTR_ALIGN(8) senderStack[THREAD_STACK_SIZE];
static u8 CTR_ALIGN(8) PXISRV11HandlerStack[THREAD_STACK_SIZE];
static MyThread receiverThread = {0}, senderThread = {0}, PXISRV11HandlerThread = {0};

Result __sync_init(void);
Result __sync_fini(void);
void __libc_fini_array(void);
void __libc_init_array(void);

void __ctru_exit(int rc) { (void)rc; } // needed to avoid linking error

// this is called after main exits
void __wrap_exit(int rc)
{
    (void)rc;

    srvExit();
    exitPXI();

    svcCloseHandle(terminationRequestedEvent);
    svcCloseHandle(sessionManager.sendAllBuffersToArm9Event);
    svcCloseHandle(sessionManager.replySemaphore);
    svcCloseHandle(sessionManager.PXISRV11CommandReceivedEvent);
    svcCloseHandle(sessionManager.PXISRV11ReplySentEvent);

    //__libc_fini_array();
    __sync_fini();
    svcExitProcess();
}

// this is called before main

void initSystem(void)
{
    __sync_init();

    assertSuccess(svcCreateEvent(&terminationRequestedEvent, RESET_STICKY));

    assertSuccess(svcCreateEvent(&sessionManager.sendAllBuffersToArm9Event, RESET_ONESHOT));
    assertSuccess(svcCreateSemaphore(&sessionManager.replySemaphore, 0, 9));
    assertSuccess(svcCreateEvent(&sessionManager.PXISRV11CommandReceivedEvent, RESET_ONESHOT));
    assertSuccess(svcCreateEvent(&sessionManager.PXISRV11ReplySentEvent, RESET_ONESHOT));    
    initPXI();

    for(Result res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL))
    {
        res = srvInit();
        if(R_FAILED(res) && res != (Result)0xD88007FA)
            svcBreak(USERBREAK_PANIC);
    }

    //__libc_init_array();
}

int main(void)
{
    Handle handles[10] = {0}; //notification handle + service handles

    for(u32 i = 0; i < 9; i++)
        assertSuccess(srvRegisterService(handles + 1 + i, serviceNames[i], 1));

    assertSuccess(MyThread_Create(&receiverThread, receiver, receiverStack, THREAD_STACK_SIZE, 0x2D, -2));
    assertSuccess(MyThread_Create(&senderThread, sender, senderStack, THREAD_STACK_SIZE, 0x2D, -2));
    assertSuccess(MyThread_Create(&PXISRV11HandlerThread, PXISRV11Handler, PXISRV11HandlerStack, THREAD_STACK_SIZE, 0x2D, -2));

    assertSuccess(srvEnableNotification(&handles[0]));

    while(!shouldTerminate)
    {
        s32 index = 0;
        assertSuccess(svcWaitSynchronizationN(&index, handles, 10, false, -1LL));

        if(index == 0)
        {
            u32 notificationId;
            assertSuccess(srvReceiveNotification(&notificationId));
            if(notificationId == 0x100) shouldTerminate = true;
        }

        else
        {
            Handle session = 0;
            SessionData *data = &sessionManager.sessionData[index - 1];
            assertSuccess(svcAcceptSession(&session, handles[index]));

            RecursiveLock_Lock(&sessionManager.senderLock);
            if(data->handle != 0)
                svcBreak(USERBREAK_PANIC);

            data->handle = session;
            assertSuccess(svcSignalEvent(sessionManager.sendAllBuffersToArm9Event));

            RecursiveLock_Unlock(&sessionManager.senderLock);
        }

    }

    u32 PXIMC_OnPXITerminate = 0x10000; //TODO: see if this is correct
    sessionManager.sessionData[0].state = STATE_SENT_TO_ARM9;
    sendPXICmdbuf(NULL, 0, &PXIMC_OnPXITerminate);

    assertSuccess(MyThread_Join(&receiverThread, -1LL));
    assertSuccess(MyThread_Join(&senderThread, -1LL));
    assertSuccess(MyThread_Join(&PXISRV11HandlerThread, -1LL));

    for(u32 i = 0; i < 10; i++)
        svcCloseHandle(handles[i]);

    return 0;
}
