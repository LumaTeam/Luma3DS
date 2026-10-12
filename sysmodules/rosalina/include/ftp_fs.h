#pragma once

#include <3ds/types.h>

#define FTP_PATH_MAX 256
#define FTP_NAME_MAX 256

typedef struct FtpDirEntry
{
    char name[FTP_NAME_MAX];
    bool isDir;
    u64  size;
} FtpDirEntry;

// Retourne false pour interrompre le parcours
typedef bool (*FtpDirCb)(const FtpDirEntry *entry, void *user);

// Combine cwd + arg en chemin absolu normalisé (gère ".", ".." et les "/" multiples).
// "out" doit faire FTP_PATH_MAX octets. false si le chemin est trop long.
bool   ftpFsResolve(const char *cwd, const char *arg, char *out);
bool   ftpFsIsDir(const char *path);
Result ftpFsListDir(const char *path, FtpDirCb cb, void *user);