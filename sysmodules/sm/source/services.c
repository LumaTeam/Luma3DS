/*
services.c

(c) TuxSH, 2017-2020
This is part of 3ds_sm, which is licensed under the MIT license (see LICENSE for details).
*/

#include "common.h"
#include "services.h"
#include "processes.h"
#include "list.h"

ServiceInfo servicesInfo[0xA0] = { 0 };
u32 nbServices = 0; // including "ports" registered with getPort

static Result checkServiceName(const char *name, s32 nameSize)
{
    if(nameSize <= 0 || nameSize > 8)
        return 0xD9006405;
    else if(strnlen(name, nameSize) < (size_t)nameSize)
        return 0xD9006407;
    else
        return 0;
}

static inline bool areServiceNamesEqual(const char *name, const char *name2, s32 nameSize)
{
    return strncmp(name, name2, nameSize) == 0 && (nameSize == 8 || name[nameSize] == 0);
}

static s32 findServicePortByName(bool isNamedPort, const char *name, s32 nameSize)
{
    ServiceInfo *info;
    for(info = servicesInfo; info < servicesInfo + nbServices && (info->isNamedPort != isNamedPort || !areServiceNamesEqual(info->name, name, nameSize)); info++);

    return info >= servicesInfo + nbServices ? -1 : info - servicesInfo;
}

static bool checkServiceAccess(SessionData *sessionData, const char *name, s32 nameSize)
{
    (void)sessionData;
    (void)name;
    (void)nameSize;

    return true; // Service access list checks removed for Luma3DS, see original 3ds_sm for implementation details.
}

static Result doRegisterServiceOrPort(u32 pid, Handle *serverPort, Handle clientPort, const char *name, s32 nameSize, s32 maxSessions, bool isNamedPort)
{
    Result res = checkServiceName(name, nameSize);
    Handle portServer, portClient;

    if(R_FAILED(res))
        return res;
    else if(findServicePortByName(isNamedPort, name, nameSize) != -1)
        return 0xD9001BFC;

    if(nbServices >= 0xA0)
        return 0xD86067F3;

    if(!isNamedPort)
    {
        res = svcCreatePort(&portServer, &portClient, NULL, maxSessions);
        if(R_FAILED(res))
        {
            *serverPort = 0;
            return 0xD9001BFC;
        }
    }
    else
        portClient = clientPort;

    ServiceInfo *serviceInfo = &servicesInfo[nbServices++];
    strncpy(serviceInfo->name, name, 8);

    serviceInfo->pid = pid;
    serviceInfo->clientPort = portClient;
    serviceInfo->isNamedPort = isNamedPort;

    SessionData *nextSessionData;
    s32 n = 0;

    for(SessionData *node = sessionDataWaitingForServiceOrPortRegisterList.first; node != NULL; node = nextSessionData)
    {
        nextSessionData = node->next;
        if((node->replayCmdbuf[0] & 0xF0000) == (!isNamedPort ? 0x50000 : 0x80000) &&
            areServiceNamesEqual((const char *)(node->replayCmdbuf + 1), name, (s32)node->replayCmdbuf[3]))
        {
            moveNode(node, &sessionDataToWakeUpAfterServiceOrPortRegisterList, true);
            ++n;
        }
    }
    if(n > 0)
    {
        s32 count;
        assertSuccess(svcReleaseSemaphore(&count, resumeGetServiceHandleOrPortRegisteredSemaphore, n));
    }

    if(!isNamedPort)
        *serverPort = portServer;

    return res;
}

Result doRegisterService(u32 pid, Handle *serverPort, const char *name, s32 nameSize, s32 maxSessions)
{
    return doRegisterServiceOrPort(pid, serverPort, 0, name, nameSize, maxSessions, false);
}

Result RegisterService(SessionData *sessionData, Handle *serverPort, const char *name, s32 nameSize, s32 maxSessions)
{
    return doRegisterService(sessionData->pid, serverPort, name, nameSize, maxSessions);
}

Result RegisterPort(SessionData *sessionData, Handle clientPort, const char *name, s32 nameSize)
{
    return doRegisterServiceOrPort(sessionData->pid, NULL, clientPort, name, nameSize, -1, true);
}

static Result UnregisterServiceOrPort(SessionData *sessionData, const char *name, s32 nameSize, bool isNamedPort)
{
    Result res = checkServiceName(name, nameSize);
    s32 serviceId;

    if(R_FAILED(res))
        return res;

    serviceId = findServicePortByName(isNamedPort, name, nameSize);
    if(serviceId == -1)
        return 0xD8801BFA;
    else if(servicesInfo[serviceId].pid != sessionData->pid)
        return 0xD8E06406;
    else
    {
        svcCloseHandle(servicesInfo[serviceId].clientPort);

        servicesInfo[serviceId] = servicesInfo[--nbServices];
        return 0;
    }
}

Result UnregisterService(SessionData *sessionData, const char *name, s32 nameSize)
{
    return UnregisterServiceOrPort(sessionData, name, nameSize, false);
}

Result UnregisterPort(SessionData *sessionData, const char *name, s32 nameSize)
{
    return UnregisterServiceOrPort(sessionData, name, nameSize, true);
}

Result IsServiceRegistered(SessionData *sessionData, bool *isRegistered, const char *name, s32 nameSize)
{
    Result res = checkServiceName(name, nameSize);

    if(R_FAILED(res))
        return res;
    else if(!checkServiceAccess(sessionData, name, nameSize))
        return 0xD8E06406;
    else
    {
        *isRegistered = findServicePortByName(false, name, nameSize) != -1;
        return 0;
    }
}

Result GetServiceHandle(SessionData *sessionData, Handle *session, const char *name, s32 nameSize, u32 flags)
{
    Result res = checkServiceName(name, nameSize);
    s32 serviceId;
    *session = 0;

    if(R_FAILED(res))
        return res;
    else if(!checkServiceAccess(sessionData, name, nameSize))
        return 0xD8E06406;

    serviceId = findServicePortByName(false, name, nameSize);
    if(serviceId == -1)
        return 0xD0406401;
    else
    {
        Handle port = servicesInfo[serviceId].clientPort;
        res = svcCreateSessionToPort(session, port);
        if(R_FAILED(res))
            *session = 0;
        if(res == (Result)0xD0401834 && !(flags & 1))
        {
            sessionData->busyClientPortHandle = port;
            return 0xD0406402;
        }

        return res;
    }
}

Result GetPort(SessionData *sessionData, Handle *port, const char *name, s32 nameSize, u8 flags)
{
    Result res = checkServiceName(name, nameSize);
    s32 serviceId;
    *port = 0;

    if(R_FAILED(res))
        return res;
    else if(!checkServiceAccess(sessionData, name, nameSize))
        return 0xD8E06406;

    serviceId = findServicePortByName(true, name, nameSize);
    if(serviceId == -1)
        return flags != 0 ? 0xD0406401 : 0xD8801BFA;
    else if(flags == 0)
        *port = servicesInfo[serviceId].clientPort;

    return 0;
}
