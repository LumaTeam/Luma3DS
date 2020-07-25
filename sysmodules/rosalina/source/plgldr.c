#include <3ds.h>
#include "plgldr.h"
#include <string.h>

static Handle   plgLdrHandle;
static int      plgLdrRefCount;

Result  plgLdrInit(void)
{
    Result res = 0;

    if (AtomicPostIncrement(&plgLdrRefCount) == 0)
        res = svcConnectToPort(&plgLdrHandle, "plg:ldr");

    if (R_FAILED(res))
        AtomicDecrement(&plgLdrRefCount);

    return res;
}

void    plgLdrExit(void)
{
    if (AtomicDecrement(&plgLdrRefCount))
        return;

    svcCloseHandle(plgLdrHandle);
}

Result  PLGLDR__IsPluginLoaderEnabled(bool *isEnabled)
{
    Result res = 0;

    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(2, 0, 0);
    if (R_SUCCEEDED((res = svcSendSyncRequest(plgLdrHandle))))
    {
        res = cmdbuf[1];
        *isEnabled = cmdbuf[2];
    }
    return res;
}

Result  PLGLDR__SetPluginLoaderState(bool enabled)
{
    Result res = 0;

    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(3, 1, 0);
    cmdbuf[1] = (u32)enabled;

    if (R_SUCCEEDED((res = svcSendSyncRequest(plgLdrHandle))))
    {
        res = cmdbuf[1];
    }
    return res;
}

Result  PLGLDR__SetPluginLoadParameters(PluginLoadParameters *parameters)
{
    Result res = 0;

    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(4, 2, 4);
    cmdbuf[1] = (u32)parameters->noFlash;
    cmdbuf[2] = parameters->lowTitleId;
    cmdbuf[3] = IPC_Desc_Buffer(256, IPC_BUFFER_R);
    cmdbuf[4] = (u32)parameters->path;
    cmdbuf[5] = IPC_Desc_Buffer(32 * sizeof(u32), IPC_BUFFER_R);
    cmdbuf[6] = (u32)parameters->config;

    if (R_SUCCEEDED((res = svcSendSyncRequest(plgLdrHandle))))
    {
        res = cmdbuf[1];
    }
    return res;
}

Result  PLGLDR__DisplayMenu(PluginMenu *menu)
{
    Result res = 0;

    u32 nbItems = menu->nbItems;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(5, 1, 8);
    cmdbuf[1] = nbItems;
    cmdbuf[2] = IPC_Desc_Buffer(nbItems, IPC_BUFFER_RW);
    cmdbuf[3] = (u32)menu->states;
    cmdbuf[4] = IPC_Desc_Buffer(MAX_BUFFER, IPC_BUFFER_R);
    cmdbuf[5] = (u32)menu->title;
    cmdbuf[6] = IPC_Desc_Buffer(MAX_BUFFER * nbItems, IPC_BUFFER_R);
    cmdbuf[7] = (u32)menu->items;
    cmdbuf[8] = IPC_Desc_Buffer(MAX_BUFFER * nbItems, IPC_BUFFER_R);
    cmdbuf[9] = (u32)menu->hints;

    if (R_SUCCEEDED((res = svcSendSyncRequest(plgLdrHandle))))
    {
        res = cmdbuf[1];
    }
    return res;
}

Result  PLGLDR__DisplayMessage(const char *title, const char *body)
{
    Result res = 0;

    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(6, 0, 4);
    cmdbuf[1] = IPC_Desc_Buffer(strlen(title), IPC_BUFFER_R);
    cmdbuf[2] = (u32)title;
    cmdbuf[3] = IPC_Desc_Buffer(strlen(body), IPC_BUFFER_R);
    cmdbuf[4] = (u32)body;

    if (R_SUCCEEDED((res = svcSendSyncRequest(plgLdrHandle))))
    {
        res = cmdbuf[1];
    }
    return res;
}

Result  PLGLDR__DisplayErrMessage(const char *title, const char *body, u32 error)
{
    Result res = 0;

    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(7, 1, 4);
    cmdbuf[1] = error;
    cmdbuf[2] = IPC_Desc_Buffer(strlen(title), IPC_BUFFER_R);
    cmdbuf[3] = (u32)title;
    cmdbuf[4] = IPC_Desc_Buffer(strlen(body), IPC_BUFFER_R);
    cmdbuf[5] = (u32)body;

    if (R_SUCCEEDED((res = svcSendSyncRequest(plgLdrHandle))))
    {
        res = cmdbuf[1];
    }
    return res;
}
