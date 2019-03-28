#include <3ds.h>
#include "my_thread.h"

static void _thread_begin(void* arg)
{
    MyThread *t = (MyThread *)arg;
    t->ep(t->p);
    MyThread_Exit();
}

Result MyThread_Create(MyThread *t, void (*entrypoint)(void *p), void *p, void *stack, u32 stackSize, int prio, int affinity)
{
    t->ep       = entrypoint;
    t->p        = p;
    t->stacktop = (u8 *)stack + stackSize;

    return svcCreateThread(&t->handle, _thread_begin, (u32)t, (u32*)t->stacktop, prio, affinity);
}

Result MyThread_Join(MyThread *thread, s64 timeout_ns)
{
    if (thread == NULL) return 0;
    Result res = svcWaitSynchronization(thread->handle, timeout_ns);
    if(R_FAILED(res)) return res;

    svcCloseHandle(thread->handle);
    thread->handle = (Handle)0;

    return res;
}

void MyThread_Exit(void)
{
    svcExitThread();
}
