#pragma once

#include <3ds/types.h>
#include "MyThread.h"
#include "plgldr.h"

void        PluginLoader__Init(void);
bool        PluginLoader__IsEnabled(void);
void        PluginLoader__MenuCallback(void);
void        PluginLoader__UpdateMenu(void);
void        PluginLoader__HandleKernelEvent(u32 notifId);
void        PluginLoader__HandleCommands(void *ctx);

void    PluginLoader__Error(const char *message, Result res);

Result     MemoryBlock__SetSize(u32 size);
Result     MemoryBlock__IsReady(void);
Result     MemoryBlock__Free(void);
Result     MemoryBlock__ToSwapFile(void);
Result     MemoryBlock__FromSwapFile(void);
Result     MemoryBlock__MountInProcess(void);
Result     MemoryBlock__UnmountFromProcess(void);
Result     MemoryBlock__SetSwapSettings(u32* func, bool isDec, u32* params);
void       MemoryBlock__ResetSwapSettings(void);

extern u32  g_loadSaveSwapArgs[0x4];
extern u32  g_loadExeArgs[0x4];
extern char g_swapFileName[256];
extern u32  g_memBlockSize;

u32         saveSwapFunc(void* startAddr, void* endAddr, void* args);
u32         loadSwapFunc(void* startAddr, void* endAddr, void* args);
u32		    loadExeFunc(void* startAddr, void* endAddr, void* args);

bool     TryToLoadPlugin(Handle process);
void    PLG__NotifyEvent(PLG_Event event, bool signal);

typedef enum
{
    PLG_CFG_NONE = 0,
    PLG_CFG_RUNNING = 1,
    PLG_CFG_SWAPPED = 2,

    PLG_CFG_SWAP_EVENT = 1 << 16,
    PLG_CFG_EXIT_EVENT = 2 << 16
}   PLG_CFG_STATUS;

typedef struct
{
    bool    isReady;
    u8 *    memblock;
}   MemoryBlock;

typedef struct
{
    Result          code;
    const char *    message;
}   Error;

typedef struct 
{
    bool            isEnabled;
    bool            pluginIsSwapped;
    const char *    pluginPath;
    MemoryBlock     memblock;
    Error           error;
    PluginHeader    header;
    Handle          target;
    Handle          arbiter;
    Handle          kernelEvent;

    bool            useUserLoadParameters;
    PluginLoadParameters    userLoadParameters;

    s32          plgEvent;
    s32          plgReply;
    s32 *        plgEventPA;
    s32 *        plgReplyPA;
    
    bool            isExeLoadFunctionset;
    bool            isSwapFunctionset;
    u32             exeLoadChecksum;
    u32             swapLoadChecksum;    
}   PluginLoaderContext;

extern PluginLoaderContext PluginLoaderCtx;