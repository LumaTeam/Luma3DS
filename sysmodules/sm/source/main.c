/*
main.c

(c) TuxSH, 2017
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "common.h"
#include "services.h"
#include "processes.h"
#include "srv.h"
#include "srv_pm.h"
#include "list.h"

u32 nbSection0Modules;
Handle resumeGetServiceHandleOrPortRegisteredSemaphore;

SessionDataList sessionDataInUseList = {NULL, NULL}, freeSessionDataList = {NULL, NULL};
SessionDataList sessionDataWaitingForServiceOrPortRegisterList = {NULL, NULL}, sessionDataToWakeUpAfterServiceOrPortRegisterList = {NULL, NULL};
SessionDataList sessionDataWaitingPortReadyList = {NULL, NULL};

static SessionData sessionDataPool[76];
static ProcessData processDataPool[64];

static u8 ALIGN(4) serviceAccessListStaticBuffer[0x110];

void __appInit(void)
{
    s64 out;

    u32 *staticBuffers = getThreadStaticBuffers();
    staticBuffers[0] = IPC_Desc_StaticBuffer(0x110, 0);
    staticBuffers[1] = (u32)serviceAccessListStaticBuffer;

    svcGetSystemInfo(&out, 26, 0);
    nbSection0Modules = out;
    assertSuccess(svcCreateSemaphore(&resumeGetServiceHandleOrPortRegisteredSemaphore, 0, 64));

    buildList(&freeSessionDataList, sessionDataPool, sizeof(sessionDataPool) / sizeof(SessionData), sizeof(SessionData));
    buildList(&freeProcessDataList, processDataPool, sizeof(processDataPool) / sizeof(ProcessData), sizeof(ProcessData));
}


// this is called after main exits
void __appExit(void){}

void __system_allocateHeaps(void){}

void __system_initSyscalls(void){}

Result __sync_init(void);
Result __sync_fini(void);

void __ctru_exit(void){}

void initSystem(void)
{
    void __libc_init_array(void);
    __sync_init();
    __system_allocateHeaps();
    __appInit();
    __libc_init_array();
}

int main(void)
{
    Result res;
    u32 *cmdbuf = getThreadCommandBuffer();
    u32 nbHandles = 3, nbSessions = 0, nbSrvPmSessions = 0;

    Handle clientPortDummy;
    Handle srvPort, srvPmPort;
    Handle handles[0xE3] = { 0 };
    Handle replyTarget = 0;

    u32 smPid;
    SessionData *sessionData;

    assertSuccess(svcGetProcessId(&smPid, CUR_PROCESS_HANDLE));
    assertSuccess(svcCreatePort(&srvPort, &clientPortDummy, "srv:", 64));

    if(IS_PRE_7X)
        assertSuccess(svcCreatePort(&srvPmPort, &clientPortDummy, "srv:pm", 64));
    else
        assertSuccess(doRegisterService(smPid, &srvPmPort, "srv:pm", 6, 64));

    handles[0] = resumeGetServiceHandleOrPortRegisteredSemaphore;
    handles[1] = srvPort;
    handles[2] = srvPmPort;

    for(;;)
    {
        s32 id;
        if(replyTarget == 0)
            cmdbuf[0] = 0xFFFF0000; // Kernel11

        res = svcReplyAndReceive(&id, handles, nbHandles, replyTarget);
        if(res == (Result)0xC920181A) // unreachable remote
        {
            // Note: if a process has ended, pm will call UnregisterProcess on it
            if(id < 0)
            {
                for(id = 3; (u32)id < nbHandles && handles[id] != replyTarget; id++);
                if((u32)id >= nbHandles)
                    panic();
            }

            if(id < 3)
                panic();
            else if((u32)id < 3 + nbSessions) // Session closed
            {
                sessionData = NULL;
                for(u32 i = 0; i < sizeof(sessionDataPool) / sizeof(SessionData); i++)
                {
                    if(sessionDataPool[i].handle == handles[id])
                        sessionData = &sessionDataPool[i];
                }

                if(sessionData != NULL)
                {
                    if(sessionData->busyClientPortHandle != 0) // remove unreferenced client port handles from array
                    {
                        u32 refCount = 0;
                        for(u32 i = 0; i < sizeof(sessionDataPool) / sizeof(SessionData); i++)
                        {
                            if(sessionDataPool[i].busyClientPortHandle == sessionData->busyClientPortHandle)
                                ++refCount;
                        }
                        if(refCount <= 1)
                        {
                            u32 i;
                            for(i = 3 + nbSessions; i < nbHandles && handles[i] == sessionData->busyClientPortHandle; i++);
                            if(i < nbHandles)
                                handles[i] = handles[--nbHandles];
                        }
                    }

                    --nbHandles;
                    for(u32 i = (u32)id; i < nbHandles; i++)
                        handles[i] = handles[i + 1];
                    --nbSessions;
                    svcCloseHandle(sessionData->handle);
                    if(sessionData->isSrvPm)
                        --nbSrvPmSessions;
                    moveNode(sessionData, &freeSessionDataList, false);
                }
                else
                    panic();
            }
            else // Port closed
            {
                SessionData *nextSessionData = NULL;

                // Update the command postponing reason accordingly
                for(sessionData = sessionDataWaitingPortReadyList.first; sessionData != NULL; sessionData = nextSessionData)
                {
                    nextSessionData = sessionData->next;
                    if(sessionData->busyClientPortHandle == handles[id])
                    {
                        sessionData->replayCmdbuf[1] = 0xD0406401; // unregistered service or named port
                        moveNode(sessionData, &sessionDataWaitingForServiceOrPortRegisterList, true);
                        svcCloseHandle(handles[id]);
                        handles[id] = handles[--nbHandles];
                        sessionData->busyClientPortHandle = 0;
                    }
                }
            }

            replyTarget = 0;
        }
        else if(R_FAILED(res))
            panic();
        else
        {
            replyTarget = 0;
            if(id == 1) // New srv: session
            {
                Handle session;
                assertSuccess(svcAcceptSession(&session, srvPort));
                sessionData = (SessionData *)allocateNode(&sessionDataInUseList, &freeSessionDataList, sizeof(SessionData), false);
                sessionData->pid = (u32)-1;
                sessionData->handle = session;
                for(u32 i = nbHandles; i > 3 + nbSessions; i--)
                    handles[i] = handles[i - 1];
                handles[3 + nbSessions++] = session;
                ++nbHandles;
            }
            else if(id == 2) // New srv:pm session
            {
                Handle session;
                if(!IS_PRE_7X && nbSrvPmSessions >= 1)
                    panic();
                assertSuccess(svcAcceptSession(&session, srvPmPort));
                sessionData = (SessionData *)allocateNode(&sessionDataInUseList, &freeSessionDataList, sizeof(SessionData), false);
                sessionData->pid = (u32)-1;
                sessionData->handle = session;
                sessionData->isSrvPm = true;
                for(u32 i = nbHandles; i > 3 + nbSessions; i--)
                    handles[i] = handles[i - 1];
                handles[3 + nbSessions++] = session;
                ++nbHandles;
                ++nbSrvPmSessions;
            }
            else
            {
                if(id == 0) // Resume SRV:GetServiceHandle or GetPort due to service or named port not registered
                {
                    if(sessionDataToWakeUpAfterServiceOrPortRegisterList.first == NULL)
                        panic();
                    sessionData = sessionDataToWakeUpAfterServiceOrPortRegisterList.first;
                    moveNode(sessionData, &sessionDataInUseList, false);
                    for(u32 i = nbHandles; i > 3 + nbSessions; i--)
                        handles[i] = handles[i - 1];
                    handles[3 + nbSessions++] = sessionData->handle;
                    ++nbHandles;
                    if(sessionData->isSrvPm)
                        ++nbSrvPmSessions;
                    memcpy(cmdbuf, sessionData->replayCmdbuf, 16);
                }
                else if((u32)id >= 3 + nbSessions) // Resume SRV:GetServiceHandle if service was full
                {
                    SessionData *sTmp;
                    for(sessionData = sessionDataWaitingPortReadyList.first; sessionData != NULL && sessionData->busyClientPortHandle != handles[id];
                        sessionData = sessionData->next);
                    if(sessionData == NULL)
                        panic();
                    moveNode(sessionData, &sessionDataInUseList, false);
                    for(sTmp = sessionDataWaitingPortReadyList.first; sTmp != NULL && sTmp->busyClientPortHandle != handles[id]; sTmp = sTmp->next);
                    if(sTmp == NULL)
                        handles[id] = handles[--nbHandles];
                    for(u32 i = nbHandles + 1; i > 3 + nbSessions; i--)
                        handles[i] = handles[i - 1];
                    handles[3 + nbSessions++] = sessionData->handle;
                    ++nbHandles;
                    if(sessionData->isSrvPm)
                        ++nbSrvPmSessions;
                    memcpy(cmdbuf, sessionData->replayCmdbuf, 16);
                    sessionData->busyClientPortHandle = 0;
                }
                else
                {
                    for(sessionData = sessionDataInUseList.first; sessionData != NULL && sessionData->handle != handles[id]; sessionData = sessionData->next);
                    if(sessionData == NULL)
                        panic();
                }

                for(id = 3; (u32)id < 3 + nbSessions && handles[id] != sessionData->handle; id++);
                if((u32)id >= 3 + nbSessions)
                    panic();

                res = sessionData->isSrvPm ? srvPmHandleCommands(sessionData) : srvHandleCommands(sessionData);

                if(R_MODULE(res) == RM_SRV && R_SUMMARY(res) == RS_WOULDBLOCK)
                {
                    SessionDataList *dstList = NULL;
                    if(res == (Result)0xD0406401) // service or named port not registered yet
                        dstList = &sessionDataWaitingForServiceOrPortRegisterList;
                    else if(res == (Result)0xD0406402) // service full
                    {
                        u32 i;
                        dstList = &sessionDataWaitingPortReadyList;
                        for(i = 3 + nbSessions; i < nbHandles && handles[i] != sessionData->busyClientPortHandle; i++);
                        if(i >= nbHandles)
                            handles[nbHandles++] = sessionData->busyClientPortHandle;
                    }
                    else
                        panic();

                    --nbHandles;
                    for(u32 i = (u32)id; i < nbHandles; i++)
                        handles[i] = handles[i + 1];
                    --nbSessions;
                    if(sessionData->isSrvPm)
                        --nbSrvPmSessions;
                    moveNode(sessionData, dstList, true);
                }
                else
                    replyTarget = sessionData->handle;
            }
        }
    }
    return 0;
}
