#include <3ds.h>
#include <string.h>
#include "pxipm.h"
#include "srvsys.h"

static Handle pxipmHandle;
static int pxipmRefCount;

Result pxipmInit(void)
{
  Result ret = 0;

  if (AtomicPostIncrement(&pxipmRefCount)) return 0;

  ret = srvSysGetServiceHandle(&pxipmHandle, "PxiPM");

  if (R_FAILED(ret)) AtomicDecrement(&pxipmRefCount);
  return ret;
}

void pxipmExit(void)
{
  if (AtomicDecrement(&pxipmRefCount)) return;
  svcCloseHandle(pxipmHandle);
}

Result PXIPM_RegisterProgram(u64 *prog_handle, FS_ProgramInfo *title, FS_ProgramInfo *update)
{
  u32 *cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x2,8,0); // 0x20200
  memcpy(&cmdbuf[1], &title->programId, sizeof(u64));
  *(u8 *)&cmdbuf[3] = title->mediaType;
  memcpy(((u8 *)&cmdbuf[3])+1, &title->padding, 7);
  memcpy(&cmdbuf[5], &update->programId, sizeof(u64));
  *(u8 *)&cmdbuf[7] = update->mediaType;
  memcpy(((u8 *)&cmdbuf[7])+1, &update->padding, 7);

  Result ret = 0;
  if(R_FAILED(ret = svcSendSyncRequest(pxipmHandle))) return ret;
  *prog_handle = *(u64*)&cmdbuf[2];

  return cmdbuf[1];
}

Result PXIPM_GetProgramInfo(ExHeader *exheader, u64 prog_handle)
{
  u32 *cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x1,2,2); // 0x10082
  cmdbuf[1] = (u32) (prog_handle);
  cmdbuf[2] = (u32) (prog_handle >> 32);
  cmdbuf[3] = (0x400 << 8) | 0x4;
  cmdbuf[4] = (u32) exheader;

  Result ret = 0;
  if(R_FAILED(ret = svcSendSyncRequest(pxipmHandle))) return ret;

  return cmdbuf[1];
}

Result PXIPM_UnregisterProgram(u64 prog_handle)
{
  u32 *cmdbuf = getThreadCommandBuffer();

  cmdbuf[0] = IPC_MakeHeader(0x3,2,0); // 0x30080
  cmdbuf[1] = (u32) (prog_handle);
  cmdbuf[2] = (u32) (prog_handle >> 32);

  Result ret = 0;
  if(R_FAILED(ret = svcSendSyncRequest(pxipmHandle))) return ret;

  return cmdbuf[1];
}
