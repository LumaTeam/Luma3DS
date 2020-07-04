/*
MyThread.h:
    Small threading library, based off ctrulib.

(c) TuxSH, 2016-2020
This is part of 3ds_pxi, which is licensed under the MIT license (see LICENSE for details).
*/

#pragma once

#include <3ds/types.h>
#include <3ds/result.h>
#include <3ds/svc.h>
#include <3ds/synchronization.h>

#define THREAD_STACK_SIZE 0x1000

typedef struct MyThread
{
    Handle handle;
    void (*ep)(void);
    bool finished;
    void* stacktop;
} MyThread;

Result MyThread_Create(MyThread *t, void (*entrypoint)(void), void *stack, u32 stackSize, int prio, int affinity);
Result MyThread_Join(MyThread *thread, s64 timeout_ns);
void MyThread_Exit(void);
