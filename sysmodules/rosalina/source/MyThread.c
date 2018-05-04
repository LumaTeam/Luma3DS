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

#include "MyThread.h"
#include "memory.h"

static void _thread_begin(void* arg)
{
    MyThread *t = (MyThread *)arg;
    t->ep();
    MyThread_Exit();
}

Result MyThread_Create(MyThread *t, void (*entrypoint)(void), void *stack, u32 stackSize, int prio, int affinity)
{
    t->ep       = entrypoint;
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
