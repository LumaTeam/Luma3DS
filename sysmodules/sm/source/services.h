/*
services.h

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds.h>

#include "common.h"

typedef struct ServiceInfo
{
    char name[8];
    Handle clientPort;
    u32 pid;
    bool isNamedPort;
} ServiceInfo;

extern ServiceInfo servicesInfo[0xA0];
extern u32 nbServices;

Result doRegisterService(u32 pid, Handle *serverPort, const char *name, s32 nameSize, s32 maxSessions);
Result RegisterService(SessionData *sessionData, Handle *serverPort, const char *name, s32 nameSize, s32 maxSessions);
Result RegisterPort(SessionData *sessionData, Handle clientPort, const char *name, s32 nameSize);
Result UnregisterService(SessionData *sessionData, const char *name, s32 nameSize);
Result UnregisterPort(SessionData *sessionData, const char *name, s32 nameSize);
Result IsServiceRegistered(SessionData *SessionData, bool *isRegistered, const char *name, s32 nameSize);
Result GetServiceHandle(SessionData *sessionData, Handle *session, const char *name, s32 nameSize, u32 flags);
Result GetPort(SessionData *sessionData, Handle *port, const char *name, s32 nameSize, u8 flags);
