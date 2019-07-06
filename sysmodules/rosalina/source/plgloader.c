#include <3ds.h>
#include "3gx.h"
#include "ifile.h"
#include "utils.h" // for makeARMBranch
#include "plgloader.h"
#include "fmt.h"
#include "menu.h"
#include "menus.h"
#include "memory.h"
#include "sleep.h"

#define MEMPERM_RW (MEMPERM_READ | MEMPERM_WRITE)
#define MemBlockSize (5*1024*1024) /* 5 MiB */

typedef struct
{
    Result          code;
    const char *    message;
}   Error;

typedef struct
{
    bool    isEnabled;
    bool    noFlash;
    u32     titleid;
    char    path[256];
    u32     config[32];
}   PluginLoadParameters;

#define HeaderMagic (0x24584733) /* "3GX$" */

typedef struct
{
    u32             magic;
    u32             version;
    u32             heapVA;
    u32             heapSize;
    u32             pluginSize;
    const char*     pluginPathPA;
    u32             isDefaultPlugin;
    u32             reserved[25];
    u32             config[32];
}   PluginHeader;

static bool         g_isEnabled;
static u8  *        g_memBlock;
static char         g_path[256];
static Handle       g_process = 0;
static u32          g_codeSize;
static u32          g_heapSize;
static Error        g_error;
static PluginLoadParameters    g_userDefinedLoadParameters;

static MyThread     g_pluginLoaderThread;
static u8 ALIGN(8)  g_pluginLoaderThreadStack[0x4000];

// pluginLoader.s
void        gamePatchFunc(void);

void        PluginLoader__ThreadMain(void);
MyThread *  PluginLoader__CreateThread(void)
{
    s64 out;

    svcGetSystemInfo(&out, 0x10000, 0x102);
    g_isEnabled = out & 1;
    g_userDefinedLoadParameters.isEnabled = false;

    if(R_FAILED(MyThread_Create(&g_pluginLoaderThread, PluginLoader__ThreadMain, g_pluginLoaderThreadStack, 0x4000, 20, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &g_pluginLoaderThread;
}

bool        PluginLoader__IsEnabled(void)
{
    return g_isEnabled;
}

void        PluginLoader__MenuCallback(void)
{
    g_isEnabled = !g_isEnabled;
    SaveSettings();
    PluginLoader__UpdateMenu();
}

void        PluginLoader__UpdateMenu(void)
{
    static const char *status[2] =
    {
        "Plugin Loader: [Disabled]",
        "Plugin Loader: [Enabled]"
    };

    rosalinaMenu.items[isN3DS + 1].title = status[g_isEnabled];
}

static Result     MapPluginInProcess(Handle proc, u32 size)
{
    u32     heapsize = MemBlockSize - size;
    Result  res;
    PluginHeader *header = (PluginHeader *)g_memBlock;

    header->heapVA = 0x06000000;
    g_heapSize = header->heapSize = heapsize;
    g_codeSize = size;

    // From now on, all memory page mapped to the process should be rwx
    svcControlProcess(proc, PROCESSOP_SET_MMU_TO_RWX, 0, 0);

    // Plugin
    if (R_FAILED((res = svcMapProcessMemoryEx(proc, 0x07000000, CUR_PROCESS_HANDLE, (u32)g_memBlock, size))))
    {
        g_error.message = "Couldn't map plugin memory block";
        g_error.code = res;
        return res;
    }

    // Heap (to be used by the plugin)
    if (R_FAILED((res = svcMapProcessMemoryEx(proc, 0x06000000, CUR_PROCESS_HANDLE, (u32)g_memBlock + size, heapsize))))
    {
        g_error.message = "Couldn't map heap memory block";
        g_error.code = res;
        goto exit;
    }

    // Clear heap section
    memset32(g_memBlock + size, 0, heapsize);

exit:
    return res;
}

static u32     strlen16(const u16 *str)
{
    u32 size = 0;

    while (str && *str++) ++size;
    return size;
}

static Result   FindPluginFile(u64 tid)
{
    char                filename[256];
    u32                 entriesNb = 0;
    bool                found = false;
    Handle              dir = 0;
    Result              res;
    FS_Archive          sdmcArchive = 0;
    FS_DirectoryEntry   entries[10];

    sprintf(g_path, "/luma/plugins/%016llX", tid);

    if (R_FAILED((res =FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, "")))))
        goto exit;

    if (R_FAILED((res = FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, g_path)))))
        goto exit;

    strcat(g_path, "/");
    while (!found && R_SUCCEEDED(FSDIR_Read(dir, &entriesNb, 10, entries)))
    {
        if (entriesNb == 0)
            break;

        static const u16 *   validExtension = u"3gx";

        for (u32 i = 0; i < entriesNb; ++i)
        {
            FS_DirectoryEntry *entry = &entries[i];

            // If entry is a folder, skip it
            if (entry->attributes & 1)
                continue;

            // Check extension
            u32 size = strlen16(entry->name);
            if (size <= 5)
                continue;

            u16 *fileExt = entry->name + size - 3;

            if (memcmp(fileExt, validExtension, 3 * sizeof(u16)))
                continue;

            // Convert name from utf16 to utf8
            int units = utf16_to_utf8((u8 *)filename, entry->name, 100);
            if (units == -1)
                continue;
            filename[units] = 0;
            found = true;
            break;
        }
    }

    if (!found)
        res = MAKERESULT(28, 4, 0, 1018);
    else
    {
        u32 len = strlen(g_path);
        filename[256 - len] = 0;
        strcat(g_path, filename);
    }

    ((PluginHeader *)g_memBlock)->pluginPathPA = PA_FROM_VA_PTR(g_path);

exit:
    FSDIR_Close(dir);
    FSUSER_CloseArchive(sdmcArchive);

    return res;
}

static Result   OpenFile(IFile *file, const char *path)
{
    return IFile_Open(file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ);
}

static Result   CheckPluginCompatibility(_3gx_Header *header, u32 processTitle)
{
    static char   errorBuf[0x100];

    if (header->targets.count == 0)
        return 0;

    for (u32 i = 0; i < header->targets.count; ++i)
    {
        if (header->targets.titles[i] == processTitle)
            return 0;
    }

    sprintf(errorBuf, "The plugin - %s -\nis not compatible with this game.\n" \
                      "Contact \"%s\" for more infos.", header->infos.titleMsg, header->infos.authorMsg);
    g_error.message = errorBuf;

    return -1;
}

static bool     TryToLoadPlugin(Handle process)
{
    u64     fileSize;
    u64     tid;
    u32     procStart = 0x00100000;
    IFile   plugin;
    PluginHeader    *hdr = (PluginHeader *)g_memBlock;
    _3gx_Header     *header;
    Result  res;

    // Clear the memblock
    memset32(g_memBlock, 0, MemBlockSize);

    hdr->magic = HeaderMagic;

    // Get title id
    svcGetProcessInfo((s64 *)&tid, process, 0x10001);
    if (R_FAILED((res = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, process, procStart, 0x1000))))
    {
        g_error.message = "Couldn't map process";
        g_error.code = res;
        return false;
    }

    // Try to open plugin file
    if (g_userDefinedLoadParameters.isEnabled && (u32)tid == g_userDefinedLoadParameters.titleid)
    {
        g_userDefinedLoadParameters.isEnabled = false;
        if (OpenFile(&plugin, g_userDefinedLoadParameters.path))
            goto exitFail;
        hdr->pluginPathPA = PA_FROM_VA_PTR(g_userDefinedLoadParameters.path);
        memcpy(hdr->config, g_userDefinedLoadParameters.config, 32 * sizeof(u32));
    }
    else
    {
        if (R_FAILED(FindPluginFile(tid)) || OpenFile(&plugin, g_path))
        {
            // Try to open default plugin
            const char *defaultPath = "/luma/plugins/default.3gx";
            if (OpenFile(&plugin, defaultPath))
                goto exitFail;
            hdr->isDefaultPlugin = 1;
            hdr->pluginPathPA = PA_FROM_VA_PTR(defaultPath);
        }
    }

    if (R_FAILED((res = IFile_GetSize(&plugin, &fileSize))))
        g_error.message = "Couldn't get file size";

    // Plugins will rarely exceed 1MB so this is fine
    header = (_3gx_Header *)(g_memBlock + MemBlockSize - (u32)fileSize);

    // Read header
    if (!res && R_FAILED((res = Read_3gx_Header(&plugin, header))))
        g_error.message = "Couldn't read file";

    // Check titles compatibility
    if (!res) res = CheckPluginCompatibility(header, (u32)tid);

    // Read code
    if (!res && R_FAILED(res = Read_3gx_Code(&plugin, header, g_memBlock + sizeof(PluginHeader))))
        g_error.message = "Couldn't read plugin's code";

    // Close file
    IFile_Close(&plugin);
    if (R_FAILED(res))
    {
        g_error.code = res;
        goto exitFail;
    }

    hdr->version = header->version;
    // Code size must be page aligned
    fileSize = (header->codeSize + 0x1100) & ~0xFFF;

    if (MapPluginInProcess(process, fileSize) == 0)
    // Install hook
    {
        extern u32  g_savedGameInstr[2];
        u32  *game = (u32 *)procStart;

        g_savedGameInstr[0] = game[0];
        g_savedGameInstr[1] = game[1];

        game[0] = 0xE51FF004; // ldr pc, [pc, #-4]
        game[1] = (u32)PA_FROM_VA_PTR(gamePatchFunc);
    }
    else
        goto exitFail;

    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, 0x1000);
    return true;

exitFail:
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, procStart, 0x1000);
    return false;
}

static void     SetKernelConfigurationMemoryFlag(bool loaded)
{
    u32 *flag = (u32 *)PA_FROM_VA_PTR(0x1FF800F0);

    *flag = loaded;
}

static void     PluginLoader_HandleCommands(void)
{
    u32    *cmdbuf = getThreadCommandBuffer();

    switch (cmdbuf[0] >> 16)
    {
        case 1: // Load plugin
        {
            if (cmdbuf[0] != IPC_MakeHeader(1, 0, 2))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            Handle process = cmdbuf[2];

            if (g_isEnabled && TryToLoadPlugin(process))
            {
                if (!g_userDefinedLoadParameters.isEnabled && g_userDefinedLoadParameters.noFlash)
                    g_userDefinedLoadParameters.noFlash = false;
                else
                {
                    for (u32 i = 0; i < 64; i++)
                    {
                        REG32(0x10202204) = 0x01FF9933;
                        svcSleepThread(5000000);
                    }
                    REG32(0x10202204) = 0;
                }

                g_process = process;
            }
            else
                svcCloseHandle(process);

            cmdbuf[0] = IPC_MakeHeader(1, 1, 0);
            cmdbuf[1] = 0;
            break;
        }

        case 2: // Check if plugin loader is enabled
        {
            if (cmdbuf[0] != IPC_MakeHeader(2, 0, 0))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            cmdbuf[0] = IPC_MakeHeader(2, 2, 0);
            cmdbuf[1] = 0;
            cmdbuf[2] = (u32)g_isEnabled;
            break;
        }

        case 3: // Enable / Disable plugin loader
        {
            if (cmdbuf[0] != IPC_MakeHeader(3, 1, 0))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            if (cmdbuf[1] != g_isEnabled)
            {
                g_isEnabled = cmdbuf[1];
                SaveSettings();
            }

            cmdbuf[0] = IPC_MakeHeader(3, 1, 0);
            cmdbuf[1] = 0;
            break;
        }

        case 4: // Define next plugin load settings
        {
            if (cmdbuf[0] != IPC_MakeHeader(4, 2, 4))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            g_userDefinedLoadParameters.isEnabled = true;
            g_userDefinedLoadParameters.noFlash = cmdbuf[1];
            g_userDefinedLoadParameters.titleid = cmdbuf[2];
            strncpy(g_userDefinedLoadParameters.path, (const char *)cmdbuf[4], 255);
            memcpy(g_userDefinedLoadParameters.config, (void *)cmdbuf[6], 32 * sizeof(u32));

            cmdbuf[0] = IPC_MakeHeader(4, 1, 0);
            cmdbuf[1] = 0;
            break;
        }

        case 5: // Display menu
        {
            if (cmdbuf[0] != IPC_MakeHeader(5, 1, 8))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            u32 nbItems = cmdbuf[1];
            u32 states = cmdbuf[3];
            DisplayPluginMenu(cmdbuf);

            cmdbuf[0] = IPC_MakeHeader(5, 1, 2);
            cmdbuf[1] = 0;
            cmdbuf[2] = IPC_Desc_Buffer(nbItems, IPC_BUFFER_RW);
            cmdbuf[3] = states;
            break;
        }

        case 6: // Display message
        {
            if (cmdbuf[0] != IPC_MakeHeader(6, 0, 4))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            const char *title = (const char *)cmdbuf[2];
            const char *body = (const char *)cmdbuf[4];

            DispMessage(title, body);

            cmdbuf[0] = IPC_MakeHeader(6, 1, 0);
            cmdbuf[1] = 0;
            break;
        }

        case 7: // Display error message
        {
            if (cmdbuf[0] != IPC_MakeHeader(7, 1, 4))
            {
                error(cmdbuf, 0xD9001830);
                break;
            }

            const char *title = (const char *)cmdbuf[3];
            const char *body = (const char *)cmdbuf[5];

            DispErrMessage(title, body, cmdbuf[1]);

            cmdbuf[0] = IPC_MakeHeader(7, 1, 0);
            cmdbuf[1] = 0;
            break;
        }
    }
}

void        PluginLoader__ThreadMain(void)
{
    const char *title = "Plugin loader";

    Result  res = 0;
    MemOp   memRegion = isN3DS ? MEMOP_REGION_BASE : MEMOP_REGION_SYSTEM;
    Handle  handles[3];
    Handle  serverHandle, clientHandle, sessionHandle = 0;

    u32 *cmdbuf = getThreadCommandBuffer();
    u32 replyTarget = 0;
    u32 nbHandle;
    s32 index;

    // Wait for the system to be completely started
    {
        bool isAcuRegistered = false;

        while (true)
        {
            if (R_SUCCEEDED(srvIsServiceRegistered(&isAcuRegistered, "ac:u"))
                && isAcuRegistered)
                break;
            svcSleepThread(100000);
        }
    }

    // Init memory block to hold the plugin
    {
        u32 free = (u32)osGetMemRegionFree(memRegion);
        u32 temp = 0;

        svcControlMemoryEx(&temp, 0x00100000, 0, free - MemBlockSize, memRegion | MEMOP_ALLOC, MEMPERM_RW, true);
        if (R_FAILED((res = svcControlMemoryEx((u32 *)&g_memBlock, 0x07000000, 0, MemBlockSize, memRegion | MEMOP_ALLOC, MEMPERM_RW, true))))
        {
            svcSleepThread(5000000000ULL); ///< Wait until the system started to display the error
            DispErrMessage(title, "Couldn't allocate memblock", res);
            return;
        }
        svcControlMemoryEx(&temp, (u32)temp, 0, MemBlockSize, memRegion | MEMOP_FREE, 0, true);
    }

    assertSuccess(svcCreatePort(&serverHandle, &clientHandle, "plg:ldr", 1));

    do
    {
        g_error.message = NULL;
        g_error.code = 0;
        handles[0] = serverHandle;
        handles[1] = sessionHandle == 0 ? g_process : sessionHandle;
        handles[2] = g_process;

        if(replyTarget == 0) // k11
            cmdbuf[0] = 0xFFFF0000;

        nbHandle = 1 + (sessionHandle != 0) + (g_process != 0);
        res = svcReplyAndReceive(&index, handles, nbHandle, replyTarget);

        if(R_FAILED(res))
        {
            if((u32)res == 0xC920181A) // session closed by remote
            {
                svcCloseHandle(sessionHandle);
                sessionHandle = 0;
                replyTarget = 0;
            }
            else
                svcBreak(USERBREAK_PANIC);
        }
        else
        {
            if(index == 0)
            {
                Handle session;
                assertSuccess(svcAcceptSession(&session, serverHandle));

                if(sessionHandle == 0)
                    sessionHandle = session;
                else
                    svcCloseHandle(session);
            }
            else if (index == 1 && handles[1] == sessionHandle)
            {
                PluginLoader_HandleCommands();
                replyTarget = sessionHandle;
            }
            else ///< The process in which we injected the plugin is terminating
            {
                // Unmap plugin's memory before closing the process
                svcUnmapProcessMemoryEx(g_process, 0x07000000, g_codeSize);
                svcUnmapProcessMemoryEx(g_process, 0x06000000, g_heapSize);
                svcCloseHandle(g_process);
                g_process = 0;
            }
        }

        if (g_error.message != NULL)
            DispErrMessage(title, g_error.message, g_error.code);

        SetKernelConfigurationMemoryFlag(g_process != 0);

    } while(!terminationRequest);

    svcCloseHandle(sessionHandle);
    svcCloseHandle(clientHandle);
    svcCloseHandle(serverHandle);
    svcControlMemoryEx((u32 *)&g_memBlock, (u32)g_memBlock, 0, MemBlockSize, memRegion | MEMOP_FREE, 0, true);
}
