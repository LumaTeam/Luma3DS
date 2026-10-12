/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
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
*
* Written by Saiitanaa
*/

#include <3ds.h>
#include <string.h>
#include <netinet/in.h>
#include "ftp.h"
#include "ftp_fs.h"
#include "minisoc.h"
#include "MyThread.h"
#include "sleep.h"
#include "sock_util.h"

#define FTP_THREAD_STACK_SIZE   0x4000
#define FTP_THREAD_PRIORITY     0x30
#define FTP_THREAD_AFFINITY     1       // CORE_SYSTEM
#define FTP_POLL_TIMEOUT_MS     100
#define FTP_DATA_TIMEOUT_MS     15000
#define FTP_CMD_BUF_SIZE        512
#define FTP_MAX_SESSIONS        2
#define FTP_MAX_LOGIN_FAILS     3
#define FTP_PASSWORD_LEN        6
#define FTP_PASV_PORT_MIN       5001
#define FTP_PASV_PORT_COUNT     64
#define FTP_PASV_TRIES          8
#define FTP_LIST_LINE_MAX       320

#define FTP_HTONS(x) ((u16)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))

extern bool preTerminationRequested;
#define FTP_SHOULD_STOP() (ftpStopRequested || preTerminationRequested)

typedef struct FtpSession
{
    int  ctrlFd;
    int  pasvFd;
    bool closing;
    bool userGiven;
    bool authed;
    u32  loginFails;
    u32  cmdLen;
    char cwd[FTP_PATH_MAX];
    char cmdBuf[FTP_CMD_BUF_SIZE];
} FtpSession;

typedef struct FtpListCtx
{
    int  fd;
    bool namesOnly;
    bool failed;
    u32  len;
    char buf[0x600];
} FtpListCtx;

static MyThread ftpThread;
static u8 CTR_ALIGN(8) ftpThreadStack[FTP_THREAD_STACK_SIZE];
static FtpSession sessions[FTP_MAX_SESSIONS];

static bool ftpThreadCreated = false;
static volatile bool     ftpStopRequested = false;
static volatile u32 ftpAcceptCount = 0;
static volatile FtpState ftpState = FTP_STATE_STOPPED;
static volatile u32      ftpIp = 0;
static volatile Result   ftpError = 0;
//static char ftpPassword[FTP_PASSWORD_LEN + 1];
static u32  ftpNextPasvPort = 0;

// ---------------------------------------------------------------- utilitaires

static char *ftpPutStr(char *p, const char *s)
{
    while(*s != 0)
        *p++ = *s++;
    return p;
}

static char *ftpPutUint(char *p, u32 v)
{
    char tmp[10];
    u32 n = 0;

    do
    {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while(v != 0);

    while(n > 0)
        *p++ = tmp[--n];
    return p;
}

// 1 = prêt, 0 = timeout, -1 = erreur
static int ftpPoll(int fd, int events, int timeoutMs)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;

    int r = socPoll(&pfd, 1, timeoutMs);
    if(r < 0)
        return -1;
    return (r > 0 && pfd.revents != 0) ? 1 : 0;
}

static bool ftpSendAll(int fd, const char *buf, u32 len)
{
    u32 sent = 0;
    u32 idle = 0;

    while(sent < len)
    {
        if(FTP_SHOULD_STOP())
            return false;

        int pr = ftpPoll(fd, POLLOUT, FTP_POLL_TIMEOUT_MS);
        if(pr < 0)
            return false;
        if(pr == 0)
        {
            idle += FTP_POLL_TIMEOUT_MS;
            if(idle >= FTP_DATA_TIMEOUT_MS)
                return false;
            continue;
        }
        idle = 0;

        u32 chunk = len - sent;
        if(chunk > 0x1000)
            chunk = 0x1000;

        int r = (int)socSend(fd, buf + sent, chunk, 0);
        if(r <= 0)
            return false;
        sent += (u32)r;
    }

    return true;
}

static void ftpSendStr(FtpSession *s, const char *str)
{
    if(!ftpSendAll(s->ctrlFd, str, (u32)strlen(str)))
        s->closing = true;
}

// Les sockets peuvent être épuisés (jeu, GDB...) : on réessaie un peu, comme sock_util.c
static int ftpCreateSocket(void)
{
    for(u32 tries = 0; tries < 10 && !FTP_SHOULD_STOP(); tries++)
    {
        int fd = socSocket(AF_INET, SOCK_STREAM, 0);
        if(fd >= 0 || fd < -10000)
            return fd;
        svcSleepThread(100 * 1000 * 1000LL);
    }
    return -1;
}

// Compare insensible à la casse du 1er mot de "line" avec "cmd" (en majuscules)
static bool ftpCmdIs(const char *line, const char *cmd)
{
    while(*cmd != 0)
    {
        char c = *line++;
        if(c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if(c != *cmd++)
            return false;
    }
    return *line == 0 || *line == ' ';
}

static const char *ftpGetArg(const char *line)
{
    while(*line != 0 && *line != ' ')
        line++;
    while(*line == ' ')
        line++;
    return line;
}

// ------------------------------------------------------------------- sessions

static void ftpSessionInit(FtpSession *s)
{
    s->ctrlFd = -1;
    s->pasvFd = -1;
    s->closing = false;
    s->userGiven = false;
    s->authed = false;
    s->loginFails = 0;
    s->cmdLen = 0;
    s->cwd[0] = '/';
    s->cwd[1] = 0;
}

static void ftpClosePasv(FtpSession *s)
{
    if(s->pasvFd >= 0)
        socClose(s->pasvFd);
    s->pasvFd = -1;
}

static void ftpSessionClose(FtpSession *s)
{
    ftpClosePasv(s);
    if(s->ctrlFd >= 0)
        socClose(s->ctrlFd);
    ftpSessionInit(s);
}

// --------------------------------------------------------- connexion de données

static bool ftpOpenPasv(FtpSession *s, u32 *portOut)
{
    ftpClosePasv(s);

    for(u32 attempt = 0; attempt < FTP_PASV_TRIES; attempt++)
    {
        u16 port = (u16)(FTP_PASV_PORT_MIN + (ftpNextPasvPort++ % FTP_PASV_PORT_COUNT));

        int fd = ftpCreateSocket();
        if(fd < 0)
            return false;

        int one = 1;
        socSetsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = FTP_HTONS(port);
        addr.sin_addr.s_addr = ftpIp;

        if(socBind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0 && socListen(fd, 1) == 0)
        {
            s->pasvFd = fd;
            *portOut = port;
            return true;
        }

        socClose(fd);
    }

    return false;
}

static void ftpCmdPasv(FtpSession *s, bool extended)
{
    u32 port = 0;
    if(!ftpOpenPasv(s, &port))
    {
        ftpSendStr(s, "425 Can't open passive connection\r\n");
        return;
    }

    char reply[80];
    char *p = reply;

    if(extended)
    {
        p = ftpPutStr(p, "229 Entering Extended Passive Mode (|||");
        p = ftpPutUint(p, port);
        p = ftpPutStr(p, "|)\r\n");
    }
    else
    {
        u32 ip = ftpIp;
        p = ftpPutStr(p, "227 Entering Passive Mode (");
        p = ftpPutUint(p, ip & 0xFF);
        *p++ = ',';
        p = ftpPutUint(p, (ip >> 8) & 0xFF);
        *p++ = ',';
        p = ftpPutUint(p, (ip >> 16) & 0xFF);
        *p++ = ',';
        p = ftpPutUint(p, (ip >> 24) & 0xFF);
        *p++ = ',';
        p = ftpPutUint(p, port >> 8);
        *p++ = ',';
        p = ftpPutUint(p, port & 0xFF);
        p = ftpPutStr(p, ")\r\n");
    }

    *p = 0;
    ftpSendStr(s, reply);
}

// Attend la connexion du client sur le socket passif ; renvoie le fd de données ou -1
static int ftpOpenData(FtpSession *s)
{
    if(s->pasvFd < 0)
    {
        ftpSendStr(s, "425 Use PASV or EPSV first\r\n");
        return -1;
    }

    int fd = -1;

    for(u32 waited = 0; waited < FTP_DATA_TIMEOUT_MS && !FTP_SHOULD_STOP(); waited += FTP_POLL_TIMEOUT_MS)
    {
        int pr = ftpPoll(s->pasvFd, POLLIN, FTP_POLL_TIMEOUT_MS);
        if(pr < 0)
            break;
        if(pr > 0)
        {
            fd = socAccept(s->pasvFd, NULL, NULL);
            break;
        }
    }

    ftpClosePasv(s);

    if(fd < 0)
        ftpSendStr(s, "425 Can't open data connection\r\n");
    return fd;
}

// -------------------------------------------------------------------- listing

static bool ftpListCb(const FtpDirEntry *e, void *user)
{
    FtpListCtx *c = (FtpListCtx *)user;

    if(FTP_SHOULD_STOP())
    {
        c->failed = true;
        return false;
    }

    if(c->len + FTP_LIST_LINE_MAX > sizeof(c->buf))
    {
        if(!ftpSendAll(c->fd, c->buf, c->len))
        {
            c->failed = true;
            return false;
        }
        c->len = 0;
    }

    char *p = c->buf + c->len;

    if(!c->namesOnly)
    {
        p = ftpPutStr(p, e->isDir ? "drwxr-xr-x 1 3ds 3ds " : "-rw-r--r-- 1 3ds 3ds ");
        p = ftpPutUint(p, (u32)e->size);
        p = ftpPutStr(p, " Jan  1  2000 "); // le FS ne fournit pas de date ici
    }

    p = ftpPutStr(p, e->name);
    *p++ = '\r';
    *p++ = '\n';

    c->len = (u32)(p - c->buf);
    return true;
}

static void ftpCmdList(FtpSession *s, const char *arg, bool namesOnly)
{
    char path[FTP_PATH_MAX];

    while(arg[0] == '-') // options de type "-a" : ignorées
    {
        while(*arg != 0 && *arg != ' ')
            arg++;
        while(*arg == ' ')
            arg++;
    }

    if(!ftpFsResolve(s->cwd, arg, path) || !ftpFsIsDir(path))
    {
        ftpSendStr(s, "550 Directory not found\r\n");
        return;
    }

    int dataFd = ftpOpenData(s);
    if(dataFd < 0)
        return;

    ftpSendStr(s, "150 Opening data connection\r\n");

    FtpListCtx ctx;
    ctx.fd = dataFd;
    ctx.namesOnly = namesOnly;
    ctx.failed = false;
    ctx.len = 0;

    Result res = ftpFsListDir(path, ftpListCb, &ctx);

    if(!ctx.failed && ctx.len > 0 && !ftpSendAll(dataFd, ctx.buf, ctx.len))
        ctx.failed = true;

    socClose(dataFd);

    if(ctx.failed || R_FAILED(res))
        ftpSendStr(s, "426 Transfer aborted\r\n");
    else
        ftpSendStr(s, "226 Transfer complete\r\n");
}

static void ftpChangeDir(FtpSession *s, const char *arg)
{
    char path[FTP_PATH_MAX];

    if(!ftpFsResolve(s->cwd, arg, path) || !ftpFsIsDir(path))
    {
        ftpSendStr(s, "550 Directory not found\r\n");
        return;
    }

    memcpy(s->cwd, path, strlen(path) + 1);
    ftpSendStr(s, "250 Directory changed\r\n");
}

// ------------------------------------------------------------------- commandes

static void ftpHandleCommand(FtpSession *s, const char *line)
{
    const char *arg = ftpGetArg(line);

    if(ftpCmdIs(line, "USER"))
        ftpSendStr(s, "331 Password required\r\n");
    else if(ftpCmdIs(line, "PASS"))
    {
        s->authed = true;
        ftpSendStr(s, "230 Logged in\r\n");
    }
    else if(ftpCmdIs(line, "QUIT"))
    {
        ftpSendStr(s, "221 Goodbye\r\n");
        s->closing = true;
    }
    else if(ftpCmdIs(line, "NOOP"))
        ftpSendStr(s, "200 OK\r\n");
    else if(ftpCmdIs(line, "SYST"))
        ftpSendStr(s, "215 UNIX Type: L8\r\n");
    else if(ftpCmdIs(line, "FEAT"))
        ftpSendStr(s, "211-Features:\r\n UTF8\r\n211 End\r\n");
    else if(ftpCmdIs(line, "OPTS"))
        ftpSendStr(s, ftpCmdIs(arg, "UTF8") ? "200 UTF8 enabled\r\n" : "501 Option not supported\r\n");
    else if(!s->authed)
        ftpSendStr(s, "530 Please login with USER and PASS\r\n");
    else if(ftpCmdIs(line, "TYPE"))
        ftpSendStr(s, "200 Type set\r\n");
    else if(ftpCmdIs(line, "PWD") || ftpCmdIs(line, "XPWD"))
    {
        char reply[FTP_PATH_MAX + 40];
        char *p = ftpPutStr(reply, "257 \"");
        p = ftpPutStr(p, s->cwd);
        p = ftpPutStr(p, "\" is the current directory\r\n");
        *p = 0;
        ftpSendStr(s, reply);
    }
    else if(ftpCmdIs(line, "CWD") || ftpCmdIs(line, "XCWD"))
        ftpChangeDir(s, arg);
    else if(ftpCmdIs(line, "CDUP") || ftpCmdIs(line, "XCUP"))
        ftpChangeDir(s, "..");
    else if(ftpCmdIs(line, "PASV"))
        ftpCmdPasv(s, false);
    else if(ftpCmdIs(line, "EPSV"))
        ftpCmdPasv(s, true);
    else if(ftpCmdIs(line, "LIST"))
        ftpCmdList(s, arg, false);
    else if(ftpCmdIs(line, "NLST"))
        ftpCmdList(s, arg, true);
    else if(ftpCmdIs(line, "PORT") || ftpCmdIs(line, "EPRT"))
        ftpSendStr(s, "502 Active mode not supported, use PASV or EPSV\r\n");
    else
        ftpSendStr(s, "502 Command not implemented\r\n");
}

static void ftpHandleInput(FtpSession *s)
{
    int r = (int)socRecv(s->ctrlFd, s->cmdBuf + s->cmdLen, FTP_CMD_BUF_SIZE - 1 - s->cmdLen, 0);
    if(r <= 0)
    {
        s->closing = true;
        return;
    }
    s->cmdLen += (u32)r;

    for(;;)
    {
        char *nl = memchr(s->cmdBuf, '\n', s->cmdLen);
        if(nl == NULL)
            break;

        u32 lineLen = (u32)(nl - s->cmdBuf);
        s->cmdBuf[lineLen] = 0;
        if(lineLen > 0 && s->cmdBuf[lineLen - 1] == '\r')
            s->cmdBuf[lineLen - 1] = 0;

        ftpHandleCommand(s, s->cmdBuf);

        u32 consumed = lineLen + 1;
        s->cmdLen -= consumed;
        memmove(s->cmdBuf, s->cmdBuf + consumed, s->cmdLen);

        if(s->closing)
            return;
    }

    if(s->cmdLen >= FTP_CMD_BUF_SIZE - 1) // ligne trop longue
    {
        s->cmdLen = 0;
        ftpSendStr(s, "500 Line too long\r\n");
    }
}

static void ftpAccept(int listenFd)
{
    int fd = socAccept(listenFd, NULL, NULL);
    if(fd < 0)
        return;
    ftpAcceptCount++;

    for(int i = 0; i < FTP_MAX_SESSIONS; i++)
    {
        FtpSession *s = &sessions[i];
        if(s->ctrlFd < 0)
        {
            ftpSessionInit(s);
            s->ctrlFd = fd;
            ftpSendStr(s, "220 Luma3DS FTP server ready\r\n");
            return;
        }
    }

    const char *msg = "421 Too many connections\r\n";
    ftpSendAll(fd, msg, (u32)strlen(msg));
    socClose(fd);
}

// ---------------------------------------------------------------------- thread

static void ftpThreadMain(void)
{
    int listenFd = -1;
    bool failed = false;
    u32 ip;
    struct sockaddr_in addr;
    int one = 1;

    Result res = miniSocInit();
    if(R_FAILED(res))
    {
        ftpError = res;
        ftpState = FTP_STATE_ERROR;
        return;
    }

    for(int i = 0; i < FTP_MAX_SESSIONS; i++)
        ftpSessionInit(&sessions[i]);

    ip = (u32)socGethostid();
    if(ip == 0 || ip == 0xFFFFFFFFu)
    {
        ftpError = FTP_ERR_NO_WIFI;
        failed = true;
        goto cleanup;
    }
    ftpIp = ip;

    listenFd = ftpCreateSocket();
    if(listenFd < 0)
    {
        ftpError = FTP_ERR_SOCKET;
        failed = true;
        goto cleanup;
    }

    socSetsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = FTP_HTONS(FTP_CONTROL_PORT);
    addr.sin_addr.s_addr = ip; // comme le serveur GDB

    if(socBind(listenFd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || socListen(listenFd, 2) != 0)
    {
        ftpError = FTP_ERR_SOCKET;
        failed = true;
        goto cleanup;
    }

    ftpState = FTP_STATE_RUNNING;

    while(!FTP_SHOULD_STOP())
    {
        struct pollfd fds[1 + FTP_MAX_SESSIONS];
        int sessIdx[1 + FTP_MAX_SESSIONS];
        nfds_t n = 0;

        if(Sleep__Status()) // bloque pendant la veille
        {
            while(!Wifi__IsConnected() && !FTP_SHOULD_STOP())
                svcSleepThread(1000 * 1000 * 1000LL);
            if(FTP_SHOULD_STOP())
                break;
        }

        fds[n].fd = listenFd;
        fds[n].events = POLLIN;
        fds[n].revents = 0;
        sessIdx[n] = -1;
        n++;

        for(int i = 0; i < FTP_MAX_SESSIONS; i++)
        {
            if(sessions[i].ctrlFd >= 0)
            {
                fds[n].fd = sessions[i].ctrlFd;
                fds[n].events = POLLIN;
                fds[n].revents = 0;
                sessIdx[n] = i;
                n++;
            }
        }

        int r = socPoll(fds, n, FTP_POLL_TIMEOUT_MS);
        if(r < -10000) // service soc cassé
        {
            ftpError = (Result)r;
            failed = true;
            break;
        }
        if(r < 0)
        {
            svcSleepThread(10 * 1000 * 1000LL);
            continue;
        }

        if(r > 0)
        {
            if(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                ftpError = FTP_ERR_SOCKET;
                failed = true;
                break;
            }
            if(fds[0].revents & POLLIN)
                ftpAccept(listenFd);

            for(nfds_t k = 1; k < n; k++)
            {
                if(fds[k].revents != 0)
                    ftpHandleInput(&sessions[sessIdx[k]]);
            }
        }

        for(int i = 0; i < FTP_MAX_SESSIONS; i++)
        {
            if(sessions[i].ctrlFd >= 0 && sessions[i].closing)
                ftpSessionClose(&sessions[i]);
        }
    }

cleanup:
    for(int i = 0; i < FTP_MAX_SESSIONS; i++)
        ftpSessionClose(&sessions[i]);
    if(listenFd >= 0)
        socClose(listenFd);

    miniSocExit();
    ftpState = failed ? FTP_STATE_ERROR : FTP_STATE_STOPPED;
}

// ------------------------------------------------------------------ API publique

/*static void ftpGeneratePassword(void)
{
    u64 t = svcGetSystemTick();
    u32 v = (u32)(t ^ (t >> 32));

    v = v * 1664525u + 1013904223u;
    v = (v ^ (v >> 15)) * 2246822519u;
    v ^= v >> 13;
    v %= 1000000u;

    for(int i = FTP_PASSWORD_LEN - 1; i >= 0; i--)
    {
        ftpPassword[i] = (char)('0' + v % 10);
        v /= 10;
    }
    ftpPassword[FTP_PASSWORD_LEN] = 0;
}*/

void ftpStop(void)
{
    if(!ftpThreadCreated)
        return;

    ftpStopRequested = true;
    Result res = MyThread_Join(&ftpThread, 3 * 1000 * 1000 * 1000LL);
    if(R_SUCCEEDED(res))
    {
        ftpThreadCreated = false;
        ftpState = FTP_STATE_STOPPED;
    }
}

void ftpStart(void)
{
    if(ftpThreadCreated)
    {
        if(ftpState == FTP_STATE_STARTING || ftpState == FTP_STATE_RUNNING)
            return;
        ftpStop(); // le thread s'est arrêté seul (erreur) : on le récupère
        if(ftpThreadCreated)
            return;
    }

    //ftpGeneratePassword();
    ftpStopRequested = false;
    ftpError = 0;
    ftpAcceptCount = 0;
    ftpIp = 0;
    ftpState = FTP_STATE_STARTING;

    Result res = MyThread_Create(&ftpThread, ftpThreadMain, ftpThreadStack, FTP_THREAD_STACK_SIZE,
                                 FTP_THREAD_PRIORITY, FTP_THREAD_AFFINITY);
    if(R_FAILED(res))
    {
        ftpError = res;
        ftpState = FTP_STATE_ERROR;
        return;
    }
    ftpThreadCreated = true;
}

FtpState    ftpGetState(void)    { return ftpState; }
u32         ftpGetIp(void)       { return ftpIp; }
u32 ftpGetAcceptCount(void) { return ftpAcceptCount; }
Result      ftpGetError(void)    { return ftpError; }
//const char *ftpGetPassword(void) { return ftpPassword; }