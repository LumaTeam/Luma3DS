#include <3ds.h>

#include "manager.h"
#include "reslimit.h"
#include "launch.h"
#include "firmlaunch.h"
#include "exheader_info_heap.h"
#include "task_runner.h"
#include "process_monitor.h"
#include "pmapp.h"
#include "pmdbg.h"
#include "util.h"
#include "my_thread.h"
#include "service_manager.h"

static MyThread processMonitorThread, taskRunnerThread;

// this is called before main
void __appInit()
{
    // Wait for sm
    for(Result res = 0xD88007FA; res == (Result)0xD88007FA; svcSleepThread(500 * 1000LL)) {
        res = srvPmInit();
        if(R_FAILED(res) && res != (Result)0xD88007FA)
            panic(res);
    }

    loaderInit();
    fsRegInit();

    static u8 ALIGN(8) processDataBuffer[0x40 * sizeof(ProcessData)] = {0};
    static u8 ALIGN(8) exheaderInfoBuffer[6 * sizeof(ExHeader_Info)] = {0};
    static u8 ALIGN(8) threadStacks[2][THREAD_STACK_SIZE] = {0};

    // Init objects
    Manager_Init(processDataBuffer, 0x40);
    ExHeaderInfoHeap_Init(exheaderInfoBuffer, 6);
    TaskRunner_Init();

    // Init the reslimits, register the KIPs and map the firmlaunch parameters
    initializeReslimits();
    Manager_RegisterKips();
    mapFirmlaunchParameters();

    // Create the threads
    assertSuccess(MyThread_Create(&processMonitorThread, processMonitor, NULL, threadStacks[0], THREAD_STACK_SIZE, 0x17, -2));
    assertSuccess(MyThread_Create(&taskRunnerThread, TaskRunner_HandleTasks, NULL, threadStacks[1], THREAD_STACK_SIZE, 0x17, -2));

    // Launch NS, etc.
    autolaunchSysmodules();
}

// this is called after main exits
void __appExit()
{
    // We don't clean up g_manager's handles because it could hang the process monitor thread, etc.
    fsRegExit();
    loaderExit();
    srvPmExit();
}


Result __sync_init(void);
Result __sync_fini(void);
void __libc_init_array(void);
void __libc_fini_array(void);

void __ctru_exit()
{
    __libc_fini_array();
    __appExit();
    __sync_fini();
    svcExitProcess();
}

void initSystem()
{
    __sync_init();
    __appInit();
    __libc_init_array();
}

static const ServiceManagerServiceEntry services[] = {
    { "pm:app",  3, pmAppHandleCommands,  false },
    { "pm:dbg",  2, pmDbgHandleCommands,  false },
    { NULL },
};

static const ServiceManagerNotificationEntry notifications[] = {
    { 0x000, NULL },
};

int main(void)
{
    Result res = 0;
    if (R_FAILED(res = ServiceManager_Run(services, notifications, NULL))) {
        panic(res);
    }
    return 0;
}
