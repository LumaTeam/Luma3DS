#pragma once

#include <3ds/types.h>
#include <3ds/synchronization.h>
#include "MyThread.h"

typedef struct TaskRunner {
    LightEvent readyEvent;
    LightEvent parametersSetEvent;
    void (*task)(void *argdata);
    u8 argStorage[0x40];
} TaskRunner;

extern TaskRunner g_taskRunner;

MyThread *taskRunnerCreateThread(void);

void TaskRunner_Init(void);
void TaskRunner_RunTask(void (*task)(void *argdata), void *argdata, size_t argsize);
/// Thread function
void TaskRunner_HandleTasks(void);
void TaskRunner_WaitReady(void);