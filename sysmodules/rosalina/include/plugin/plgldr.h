#pragma once

#include <3ds/types.h>

#define  MAX_BUFFER (50)
#define  MAX_ITEMS_COUNT (64)

#define  HeaderMagic (0x24584733) /* "3GX$" */

typedef struct
{
    bool    noFlash;
    bool    noIRPatch;
    u32     lowTitleId;
    char    path[256];
    u32     config[32];
}   PluginLoadParameters;

typedef struct
{
    u32         nbItems;
    u8          states[MAX_ITEMS_COUNT];
    char        title[MAX_BUFFER];
    char        items[MAX_ITEMS_COUNT][MAX_BUFFER];
    char        hints[MAX_ITEMS_COUNT][MAX_BUFFER];
}   PluginMenu;

typedef enum
{
    PLG_WAIT = -1,
    PLG_OK = 0,
    PLG_SLEEP_ENTRY = 1,
    PLG_SLEEP_EXIT = 2,
    PLG_ABOUT_TO_SWAP = 3,
    PLG_ABOUT_TO_EXIT = 4,
}   PLG_Event;

typedef struct
{
    u32             magic;
    u32             version;
    u32             heapVA;
    u32             heapSize;
    u32             exeSize; // Include sizeof(PluginHeader) + .text + .rodata + .data + .bss (0x1000 aligned too)
    u32             isDefaultPlugin;
    s32*            plgldrEvent; ///< Used for synchronization
    s32*            plgldrReply; ///< Used for synchronization
    u32             reserved[24];
    u32             config[32];
}   PluginHeader;

typedef void (*OnPlgLdrEventCb_t)(s32 eventType);

Result  plgLdrInit(void);
void    plgLdrExit(void);
Result  PLGLDR__IsPluginLoaderEnabled(bool *isEnabled);
Result  PLGLDR__SetPluginLoaderState(bool enabled);
Result  PLGLDR__SetPluginLoadParameters(PluginLoadParameters *parameters);
Result  PLGLDR__DisplayMenu(PluginMenu *menu);
Result  PLGLDR__DisplayMessage(const char *title, const char *body);
Result  PLGLDR__DisplayErrMessage(const char *title, const char *body, u32 error);
Result  PLGLDR__SetRosalinaMenuBlock(bool shouldBlock);
Result  PLGLDR__SetSwapSettings(char* swapPath, void* saveFunc, void* loadFunc, void* args);
Result  PLGLDR__SetExeLoadSettings(void* loadFunc, void* args);
Result  PLGLDR__GetVersion(u32 *version);
void    PLGLDR__SetEventCallback(OnPlgLdrEventCb_t cb);
void    PLGLDR__Status(void);
