#include <3ds.h>
#include <string.h>
#include "srvsys.h"

static Handle srvHandle;
static int srvRefCount;
static RecursiveLock initLock;
static int initLockinit = 0;

Result srvSysInit()
{
  Result rc = 0;

  if (!initLockinit)
  {
    RecursiveLock_Init(&initLock);
  }

  RecursiveLock_Lock(&initLock);

  if (srvRefCount > 0)
  {
    RecursiveLock_Unlock(&initLock);
    return MAKERESULT(RL_INFO, RS_NOP, 25, RD_ALREADY_INITIALIZED);
  }

  while (1)
  {
    rc = svcConnectToPort(&srvHandle, "srv:");
    if (R_LEVEL(rc) != RL_PERMANENT || 
        R_SUMMARY(rc) != RS_NOTFOUND || 
        R_DESCRIPTION(rc) != RD_NOT_FOUND
       ) break;
    svcSleepThread(500000);
  }
  if (R_SUCCEEDED(rc))
  {
    rc = srvSysRegisterClient();
    srvRefCount++;
  }

  RecursiveLock_Unlock(&initLock);
  return rc;
}

Result srvSysRegisterClient(void)
{
  Result rc = 0;
  u32* cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x1,0,2); // 0x10002
  cmdbuf[1] = IPC_Desc_CurProcessHandle();

  if(R_FAILED(rc = svcSendSyncRequest(srvHandle)))return rc;

  return cmdbuf[1];
}

Result srvSysExit()
{
  Result rc;
  RecursiveLock_Lock(&initLock);

  if (srvRefCount > 1)
  {
    srvRefCount--;
    RecursiveLock_Unlock(&initLock);
    return MAKERESULT(RL_INFO, RS_NOP, 25, RD_BUSY);
  }

  if (srvHandle != 0) svcCloseHandle(srvHandle);
  else svcBreak(USERBREAK_ASSERT);
  rc = (Result)srvHandle; // yeah, I think this is a benign bug
  srvHandle = 0;
  srvRefCount--;
  RecursiveLock_Unlock(&initLock);
  return rc;
}

Result srvSysGetServiceHandle(Handle* out, const char* name)
{
  Result rc = 0;
  u32* cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x5,4,0); // 0x50100
  strncpy((char*) &cmdbuf[1], name,8);
  cmdbuf[3] = strlen(name);
  cmdbuf[4] = 0x0;

  if(R_FAILED(rc = svcSendSyncRequest(srvHandle)))return rc;

  if(out) *out = cmdbuf[3];

  return cmdbuf[1];
}

Result srvSysEnableNotification(Handle* semaphoreOut)
{
  Result rc = 0;
  u32* cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x2,0,0);

  if(R_FAILED(rc = svcSendSyncRequest(srvHandle)))return rc;

  if(semaphoreOut) *semaphoreOut = cmdbuf[3];

  return cmdbuf[1];
}

Result srvSysReceiveNotification(u32* notificationIdOut)
{
  Result rc = 0;
  u32* cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0xB,0,0); // 0xB0000

  if(R_FAILED(rc = svcSendSyncRequest(srvHandle)))return rc;

  if(notificationIdOut) *notificationIdOut = cmdbuf[2];

  return cmdbuf[1];
}

Result srvSysRegisterService(Handle* out, const char* name, int maxSessions)
{
  Result rc = 0;
  u32* cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x3,4,0); // 0x30100
  strncpy((char*) &cmdbuf[1], name,8);
  cmdbuf[3] = strlen(name);
  cmdbuf[4] = maxSessions;

  if(R_FAILED(rc = svcSendSyncRequest(srvHandle)))return rc;

  if(out) *out = cmdbuf[3];

  return cmdbuf[1];
}

Result srvSysUnregisterService(const char* name)
{
  Result rc = 0;
  u32* cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x4,3,0); // 0x400C0
  strncpy((char*) &cmdbuf[1], name,8);
  cmdbuf[3] = strlen(name);

  if(R_FAILED(rc = svcSendSyncRequest(srvHandle)))return rc;

  return cmdbuf[1];
}
