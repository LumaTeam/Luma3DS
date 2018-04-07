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

#include "minisoc.h"
#include <sys/socket.h>
#include <3ds/ipc.h>
#include <3ds/os.h>
#include <3ds/synchronization.h>
#include "memory.h"

s32 _net_convert_error(s32 sock_retval);

static Result SOCU_Initialize(Handle memhandle, u32 memsize)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x1,1,4); // 0x10044
    cmdbuf[1] = memsize;
    cmdbuf[2] = IPC_Desc_CurProcessHandle();
    cmdbuf[4] = IPC_Desc_SharedHandles(1);
    cmdbuf[5] = memhandle;

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0)
        return ret;

    return cmdbuf[1];
}

static Result SOCU_Shutdown(void)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x19,0,0); // 0x190000

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0)
        return ret;

    return cmdbuf[1];
}

static s32 miniSocRefCount = 0;
static u32 socContextAddr = 0x08000000;
static u32 socContextSize = 0x60000;
// SOCU_handle from ctrulib
// socMemhandle from ctrulib

bool miniSocEnabled = false;

Result miniSocInit()
{
    if(AtomicPostIncrement(&miniSocRefCount))
        return 0;

    u32 tmp = 0;
    Result ret = 0;
    bool isSocURegistered;

    ret = srvIsServiceRegistered(&isSocURegistered, "soc:U");
    if(ret != 0) goto cleanup;
    if(!isSocURegistered)
    {
        ret = -1;
        goto cleanup;
    }

    ret = srvGetServiceHandle(&SOCU_handle, "soc:U");
    if(ret != 0) goto cleanup;

    ret = svcControlMemory(&tmp, socContextAddr, 0, socContextSize, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
    if(ret != 0) goto cleanup;

    socContextAddr = tmp;

    ret = svcCreateMemoryBlock(&socMemhandle, (u32)socContextAddr, socContextSize, 0, 3);
    if(ret != 0) goto cleanup;



    ret = SOCU_Initialize(socMemhandle, socContextSize);
    if(ret != 0) goto cleanup;

    svcKernelSetState(0x10000, 2);
    miniSocEnabled = true;
    return 0;

cleanup:
    AtomicDecrement(&miniSocRefCount);

    if(socMemhandle != 0)
    {
        svcCloseHandle(socMemhandle);
        socMemhandle = 0;
    }

    if(SOCU_handle != 0)
    {
        SOCU_Shutdown();
        svcCloseHandle(SOCU_handle);
        SOCU_handle = 0;
    }

    if(tmp != 0)
        svcControlMemory(&tmp, socContextAddr, socContextAddr, socContextSize, MEMOP_FREE, MEMPERM_DONTCARE);

    return ret;
}

Result miniSocExit(void)
{
    if(AtomicDecrement(&miniSocRefCount))
        return 0;

    Result ret = 0;
    u32 tmp;

    svcCloseHandle(socMemhandle);
    socMemhandle = 0;

    ret = SOCU_Shutdown();

    svcCloseHandle(SOCU_handle);
    SOCU_handle = 0;

    svcControlMemory(&tmp, socContextAddr, socContextAddr, socContextSize, MEMOP_FREE, MEMPERM_DONTCARE);
    if(ret == 0)
    {
        svcKernelSetState(0x10000, 2);
        miniSocEnabled = false;
    }
    return ret;
}

int socSocket(int domain, int type, int protocol)
{
    int ret = 0;

    u32 *cmdbuf = getThreadCommandBuffer();

    // The protocol on the 3DS *must* be 0 to work
    // To that end, when appropriate, we will make the change for the user
    if (domain == AF_INET
    && type == SOCK_STREAM
    && protocol == IPPROTO_TCP) {
        protocol = 0; // TCP is the only option, so 0 will work as expected
    }
    if (domain == AF_INET
    && type == SOCK_DGRAM
    && protocol == IPPROTO_UDP) {
        protocol = 0; // UDP is the only option, so 0 will work as expected
    }

    cmdbuf[0] = IPC_MakeHeader(0x2,3,2); // 0x200C2
    cmdbuf[1] = domain;
    cmdbuf[2] = type;
    cmdbuf[3] = protocol;
    cmdbuf[4] = IPC_Desc_CurProcessHandle();

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0)
    {
        //errno = SYNC_ERROR;
        return ret;
    }

    ret = (int)cmdbuf[1];
    if(ret == 0) ret = cmdbuf[2];
    if(ret < 0)
    {
        //if(cmdbuf[1] == 0)errno = _net_convert_error(ret);
        //if(cmdbuf[1] != 0)errno = SYNC_ERROR;
        return -1;
    }
    else
        return cmdbuf[2];
}

int socBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    Result ret = 0;
    socklen_t tmp_addrlen = 0;
    u32 *cmdbuf = getThreadCommandBuffer();
    u8 tmpaddr[0x1c];

    memset(tmpaddr, 0, 0x1c);

    if(addr->sa_family == AF_INET)
        tmp_addrlen = 8;
    else
        tmp_addrlen = 0x1c;

    if(addrlen < tmp_addrlen)
    {
        //errno = EINVAL;
        return -1;
    }

    tmpaddr[0] = tmp_addrlen;
    tmpaddr[1] = addr->sa_family;
    memcpy(&tmpaddr[2], &addr->sa_data, tmp_addrlen-2);

    cmdbuf[0] = IPC_MakeHeader(0x5,2,4); // 0x50084
    cmdbuf[1] = (u32)sockfd;
    cmdbuf[2] = (u32)tmp_addrlen;
    cmdbuf[3] = IPC_Desc_CurProcessHandle();
    cmdbuf[5] = IPC_Desc_StaticBuffer(tmp_addrlen,0);
    cmdbuf[6] = (u32)tmpaddr;

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0) {
        //errno = SYNC_ERROR;
        return ret;
    }

    ret = (int)cmdbuf[1];
    if(ret == 0)
        ret = _net_convert_error(cmdbuf[2]);

    if(ret < 0) {
        //errno = -ret;
        return -1;
    }

    return ret;
}

int socListen(int sockfd, int max_connections)
{
    Result ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x3,2,2); // 0x30082
    cmdbuf[1] = (u32)sockfd;
    cmdbuf[2] = (u32)max_connections;
    cmdbuf[3] = IPC_Desc_CurProcessHandle();

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0)
    {
        //errno = SYNC_ERROR;
        return ret;
    }

    ret = (int)cmdbuf[1];
    if(ret == 0)
        ret = _net_convert_error(cmdbuf[2]);

    if(ret < 0) {
        //errno = -ret;
        return -1;
    }

    return 0;
}

int socAccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    Result ret = 0;
    int tmp_addrlen = 0x1c;

    u32 *cmdbuf = getThreadCommandBuffer();
    u8 tmpaddr[0x1c];
    u32 saved_threadstorage[2];

    memset(tmpaddr, 0, 0x1c);

    cmdbuf[0] = IPC_MakeHeader(0x4,2,2); // 0x40082
    cmdbuf[1] = (u32)sockfd;
    cmdbuf[2] = (u32)tmp_addrlen;
    cmdbuf[3] = IPC_Desc_CurProcessHandle();

    u32 *staticbufs = getThreadStaticBuffers();
    saved_threadstorage[0] = staticbufs[0];
    saved_threadstorage[1] = staticbufs[1];

    staticbufs[0] = IPC_Desc_StaticBuffer(tmp_addrlen,0);
    staticbufs[1] = (u32)tmpaddr;

    ret = svcSendSyncRequest(SOCU_handle);

    staticbufs[0] = saved_threadstorage[0];
    staticbufs[1] = saved_threadstorage[1];

    if(ret != 0)
        return ret;

    ret = (int)cmdbuf[1];
    if(ret == 0)
        ret = _net_convert_error(cmdbuf[2]);

    if(ret < 0)
        //errno = -ret;

    if(ret >= 0 && addr != NULL)
    {
        addr->sa_family = tmpaddr[1];
        if(*addrlen > tmpaddr[0])
            *addrlen = tmpaddr[0];
        memcpy(addr->sa_data, &tmpaddr[2], *addrlen - 2);
    }

    if(ret < 0)
        return -1;

    return ret;
}

int socPoll(struct pollfd *fds, nfds_t nfds, int timeout)
{
    int ret = 0;
    nfds_t i;
    u32 size = sizeof(struct pollfd)*nfds;
    u32 *cmdbuf = getThreadCommandBuffer();
    u32 saved_threadstorage[2];

    if(nfds == 0) {
        return -1;
    }

    for(i = 0; i < nfds; ++i) {
        fds[i].revents = 0;
    }

    cmdbuf[0] = IPC_MakeHeader(0x14,2,4); // 0x140084
    cmdbuf[1] = (u32)nfds;
    cmdbuf[2] = (u32)timeout;
    cmdbuf[3] = IPC_Desc_CurProcessHandle();
    cmdbuf[5] = IPC_Desc_StaticBuffer(size,10);
    cmdbuf[6] = (u32)fds;

    u32 * staticbufs = getThreadStaticBuffers();
    saved_threadstorage[0] = staticbufs[0];
    saved_threadstorage[1] = staticbufs[1];

    staticbufs[0] = IPC_Desc_StaticBuffer(size,0);
    staticbufs[1] = (u32)fds;

    ret = svcSendSyncRequest(SOCU_handle);

    staticbufs[0] = saved_threadstorage[0];
    staticbufs[1] = saved_threadstorage[1];

    if(ret != 0) {
        return ret;
    }

    ret = (int)cmdbuf[1];
    if(ret == 0)
        ret = _net_convert_error(cmdbuf[2]);

    if(ret < 0) {
        //errno = -ret;
        return -1;
    }

    return ret;
}

int socClose(int sockfd)
{
    int ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0xB,1,2); // 0xB0042
    cmdbuf[1] = (u32)sockfd;
    cmdbuf[2] = IPC_Desc_CurProcessHandle();

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0) {
        //errno = SYNC_ERROR;
        return ret;
    }

    ret = (int)cmdbuf[1];
    if(ret == 0)
        ret =_net_convert_error(cmdbuf[2]);

    if(ret < 0) {
        //errno = -ret;
        return -1;
    }

    return 0;
}

int socSetsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    int ret = 0;
    u32 *cmdbuf = getThreadCommandBuffer();

    cmdbuf[0] = IPC_MakeHeader(0x12,4,4); // 0x120104
    cmdbuf[1] = (u32)sockfd;
    cmdbuf[2] = (u32)level;
    cmdbuf[3] = (u32)optname;
    cmdbuf[4] = (u32)optlen;
    cmdbuf[5] = IPC_Desc_CurProcessHandle();
    cmdbuf[7] = IPC_Desc_StaticBuffer(optlen,9);
    cmdbuf[8] = (u32)optval;

    ret = svcSendSyncRequest(SOCU_handle);
    if(ret != 0) {
        return ret;
    }

    ret = (int)cmdbuf[1];
    if(ret == 0)
        ret = _net_convert_error(cmdbuf[2]);

    if(ret < 0) {
        //errno = -ret;
        return -1;
    }

    return ret;
}

ssize_t soc_recv(int sockfd, void *buf, size_t len, int flags)
{
    return soc_recvfrom(sockfd, buf, len, flags, NULL, 0);
}

ssize_t soc_send(int sockfd, const void *buf, size_t len, int flags)
{
    return soc_sendto(sockfd, buf, len, flags, NULL, 0);
}
