/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2017 Aurora Wright, TuxSH
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
#include <string.h>
#include "fsreg.h"
#include "services.h"

static Handle *srvHandlePtr;
static int srvRefCount;
static RecursiveLock initLock;
static int initLockinit = 0;

Result srvSysInit(void)
{
  Result rc = 0;

  if (!initLockinit)
    RecursiveLock_Init(&initLock);

  RecursiveLock_Lock(&initLock);

  if (srvRefCount > 0)
  {
    RecursiveLock_Unlock(&initLock);
    return MAKERESULT(RL_INFO, RS_NOP, 25, RD_ALREADY_INITIALIZED);
  }

  srvHandlePtr = srvGetSessionHandle();

  while(1)
  {
    rc = svcConnectToPort(srvHandlePtr, "srv:");
    if (R_LEVEL(rc) != RL_PERMANENT ||
        R_SUMMARY(rc) != RS_NOTFOUND ||
        R_DESCRIPTION(rc) != RD_NOT_FOUND
       ) break;
    svcSleepThread(500 * 1000LL);
  }

  if(R_SUCCEEDED(rc))
  {
    rc = srvRegisterClient();
    srvRefCount++;
  }

  RecursiveLock_Unlock(&initLock);
  return rc;
}

Result srvSysExit(void)
{
  Result rc = 0;
  RecursiveLock_Lock(&initLock);

  if(srvRefCount > 1)
  {
    srvRefCount--;
    RecursiveLock_Unlock(&initLock);
    return MAKERESULT(RL_INFO, RS_NOP, 25, RD_BUSY);
  }

  if(srvHandlePtr != 0)
    svcCloseHandle(*srvHandlePtr);
  else
    svcBreak(USERBREAK_ASSERT);

  srvHandlePtr = 0;
  srvRefCount--;

  RecursiveLock_Unlock(&initLock);
  return rc;
}

void fsSysInit(void)
{
	if(R_FAILED(fsregSetupPermissions()))
		svcBreak(USERBREAK_ASSERT);

	Handle *fsHandlePtr = fsGetSessionHandle();
	srvGetServiceHandle(fsHandlePtr, "fs:USER");

	FSUSER_InitializeWithSdkVersion(*fsHandlePtr, SDK_VERSION);
	FSUSER_SetPriority(0);
}
