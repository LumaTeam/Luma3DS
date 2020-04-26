#include <3ds.h>
#include <string.h>
#include "task_runner.h"

TaskRunner g_taskRunner;

static void taskRunnerNoOpFunction(void *args)
{
    (void)args;
}

void TaskRunner_Init(void)
{
    memset(&g_taskRunner, 0, sizeof(TaskRunner));
    LightEvent_Init(&g_taskRunner.readyEvent, RESET_ONESHOT);
    LightEvent_Init(&g_taskRunner.parametersSetEvent, RESET_ONESHOT);
}

void TaskRunner_RunTask(void (*task)(void *argdata), void *argdata, size_t argsize)
{
    argsize = argsize > sizeof(g_taskRunner.argStorage) ? sizeof(g_taskRunner.argStorage) : argsize;
    LightEvent_Wait(&g_taskRunner.readyEvent);
    g_taskRunner.task = task;
    memcpy(g_taskRunner.argStorage, argdata, argsize);
    LightEvent_Signal(&g_taskRunner.parametersSetEvent);
}

void TaskRunner_Terminate(void)
{
    g_taskRunner.shouldTerminate = true;
    TaskRunner_RunTask(taskRunnerNoOpFunction, NULL, 0);
}

void TaskRunner_HandleTasks(void *p)
{
    (void)p;
    while (!g_taskRunner.shouldTerminate) {
        LightEvent_Signal(&g_taskRunner.readyEvent);
        LightEvent_Wait(&g_taskRunner.parametersSetEvent);
        g_taskRunner.task(g_taskRunner.argStorage);
    }
}

void TaskRunner_WaitReady(void)
{
    LightEvent_Wait(&g_taskRunner.readyEvent);
}
