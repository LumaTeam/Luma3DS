#pragma once

#include <3ds/types.h>

extern const u8 romfsRedirPatch[];
extern const u32 romfsRedirPatchSize;

extern u32 romfsRedirPatchSubstituted1, romfsRedirPatchHook1;
extern u32 romfsRedirPatchSubstituted2, romfsRedirPatchHook2;

extern u32 romfsRedirPatchArchiveName;
extern u32 romfsRedirPatchFsMountArchive;
extern u32 romfsRedirPatchFsRegisterArchive;
extern u32 romfsRedirPatchArchiveId;
extern u32 romfsRedirPatchRomFsMount;
extern u32 romfsRedirPatchUpdateRomFsMount;
extern u32 romfsRedirPatchCustomPath;
