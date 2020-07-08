#pragma once

#include <3ds/types.h>

typedef struct ServiceManagerServiceEntry {
    const char *name;
    u32 maxSessions;
    void (*handler)(void *ctx);
    bool isGlobalPort;
} ServiceManagerServiceEntry;

typedef struct ServiceManagerNotificationEntry {
    u32 id;
    void (*handler)(u32 id);
} ServiceManagerNotificationEntry;

typedef struct ServiceManagerContextAllocator {
    void* (*newSessionContext)(u8 serviceId);
    void  (*freeSessionContext)(void *ctx);
} ServiceManagerContextAllocator;

Result ServiceManager_Run(const ServiceManagerServiceEntry *services, const ServiceManagerNotificationEntry *notifications, const ServiceManagerContextAllocator *allocator);
