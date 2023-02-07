#pragma once
#include <3ds/types.h>
#include "gdb/server.h"
#include "gdb/debug.h"
#include "gdb/monitor.h"
#include "gdb/net.h"
#include "MyThread.h"

extern GDBServer gdbServer;
extern GDBContext *nextApplicationGdbCtx;

MyThread *debuggerCreateSocketThread(void);
MyThread *debuggerCreateDebugThread(void);
void debuggerSocketThreadMain(void);
void debuggerDebugThreadMain(void);

void debuggerFetchAndSetNextApplicationDebugHandleTask(void *argdata);

Result debuggerDisable(s64 timeout);
Result debuggerEnable(s64 timeout);
Result debugNextApplicationByForce();

// Not sure if actually needed, but I keep it as it is just in case, from my tests, simply using debugNextApplicationByForce is sufficient
void handleNextApplicationDebuggedByForce(u32 notificationId);

void handleRosalinaDebugger(void *ctx);