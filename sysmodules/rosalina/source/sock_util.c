/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#include <sys/socket.h>
#include <3ds/result.h>
#include <3ds/svc.h>
#include <3ds/synchronization.h>
#include <arpa/inet.h>
#include "memory.h"
#include "minisoc.h"
#include "sock_util.h"

extern Handle preTerminationEvent;
extern bool preTerminationRequested;

// soc's poll function is odd, and doesn't like -1 as fd.
// so this compacts everything together

static void compact(struct sock_server *serv)
{
    int new_fds[MAX_CTXS];
    struct sock_ctx *new_ctxs[MAX_CTXS];
    nfds_t n = 0;

    for(nfds_t i = 0; i < serv->nfds; i++)
    {
        if(serv->poll_fds[i].fd != -1)
        {
            new_fds[n] = serv->poll_fds[i].fd;
            new_ctxs[n] = serv->ctx_ptrs[i];
            n++;
        }
    }

    for(nfds_t i = 0; i < n; i++)
    {
        serv->poll_fds[i].fd = new_fds[i];
        serv->ctx_ptrs[i] = new_ctxs[i];
        serv->ctx_ptrs[i]->i = i;
    }
    serv->nfds = n;
    serv->compact_needed = false;
}

static struct sock_ctx *server_alloc_server_ctx(struct sock_server *serv)
{
    for(int i = 0; i < MAX_PORTS; i++)
    {
        if(serv->serv_ctxs[i].type == SOCK_NONE)
            return &serv->serv_ctxs[i];
    }

    return NULL;
}

static void server_close_ctx(struct sock_server *serv, struct sock_ctx *ctx)
{
    serv->compact_needed = true;

    Handle sock = serv->poll_fds[ctx->i].fd;
    if(ctx->type == SOCK_CLIENT)
    {
        serv->close_cb(ctx);
        serv->free(serv, ctx);
        ctx->serv->n--;
    }

    socClose(sock);
    ctx->should_close = false;

    serv->poll_fds[ctx->i].fd = -1;
    serv->poll_fds[ctx->i].events = 0;
    serv->poll_fds[ctx->i].revents = 0;

    ctx->type = SOCK_NONE;
    serv->ctx_ptrs[ctx->i] = NULL;
}

Result server_init(struct sock_server *serv)
{
    Result ret = 0;

    ret = miniSocInit();
    if(R_FAILED(ret))
        return ret;
    memset(serv, 0, sizeof(struct sock_server));

    for(int i = 0; i < MAX_PORTS; i++)
        serv->serv_ctxs[i].type = SOCK_NONE;

    for(int i = 0; i < MAX_CTXS; i++)
        serv->ctx_ptrs[i] = NULL;

    ret = svcCreateEvent(&serv->started_event, RESET_STICKY);
    if(R_FAILED(ret))
        return ret;
    return svcCreateEvent(&serv->shall_terminate_event, RESET_STICKY);
}

Result server_bind(struct sock_server *serv, u16 port)
{
    int server_sockfd;
    Handle handles[2] = { preTerminationEvent, serv->shall_terminate_event };
    s32 idx = -1;
    server_sockfd = socSocket(AF_INET, SOCK_STREAM, 0);

    int res;
    u32 tries = 15;
    while(server_sockfd == -1 && --tries > 0)
    {
        if(svcWaitSynchronizationN(&idx, handles, 2, false, 100 * 1000 * 1000LL) == 0)
            return -1;

        server_sockfd = socSocket(AF_INET, SOCK_STREAM, 0);
    }

    if (server_sockfd < -10000 || tries == 0) {
        // Socket services broken
        serv->init_result = -1;
        svcSignalEvent(serv->shall_terminate_event);
        return -1;
    }

    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = socGethostid();

    res = socBind(server_sockfd, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));
    if(res == 0)
    {
        res = socListen(server_sockfd, 2);
        if(res == 0)
        {
            int idx = serv->nfds;
            serv->nfds++;
            serv->poll_fds[idx].fd = server_sockfd;
            serv->poll_fds[idx].events = POLLIN;

            struct sock_ctx *new_ctx = server_alloc_server_ctx(serv);
            memcpy(&new_ctx->addr_in, &saddr, sizeof(struct sockaddr_in));
            new_ctx->type = SOCK_SERVER;
            new_ctx->sockfd = server_sockfd;
            new_ctx->n = 0;
            new_ctx->i = idx;
            serv->ctx_ptrs[idx] = new_ctx;
        }
    }

    if (res != 0) {
        // Socket services broken
        serv->init_result = res;
        svcSignalEvent(serv->shall_terminate_event);
        return res;
    }

    return 0;
}

static bool server_should_exit(struct sock_server *serv)
{
    return svcWaitSynchronization(serv->shall_terminate_event, 0) == 0 || svcWaitSynchronization(preTerminationEvent, 0) == 0;
}

void server_run(struct sock_server *serv)
{
    struct pollfd *fds = serv->poll_fds;

    serv->running = true;
    svcSignalEvent(serv->started_event);
    while(serv->running && !preTerminationRequested)
    {
        if(server_should_exit(serv))
            goto abort_connections;

        if(serv->nfds == 0)
        {
            svcSleepThread(12 * 1000 * 1000LL);
            continue;
        }

        for(nfds_t i = 0; i < serv->nfds; i++)
            fds[i].revents = 0;
        int pollres = socPoll(fds, serv->nfds, 50);

        if(server_should_exit(serv) || pollres < -10000)
            goto abort_connections;

        for(nfds_t i = 0; pollres > 0 && i < serv->nfds; i++)
        {
            struct sock_ctx *curr_ctx = serv->ctx_ptrs[i];

            if((fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) || curr_ctx->should_close)
                server_close_ctx(serv, curr_ctx);

            else if(fds[i].revents & POLLIN)
            {
                if(curr_ctx->type == SOCK_SERVER) // Listening socket?
                {
                    struct sockaddr_in saddr;
                    socklen_t len = sizeof(struct sockaddr_in);
                    int client_sockfd = socAccept(fds[i].fd, (struct sockaddr *)&saddr, &len);

                    if(server_should_exit(serv))
                        goto abort_connections;

                    if(client_sockfd < 0 || curr_ctx->n == serv->clients_per_server || serv->nfds == MAX_CTXS)
                        socClose(client_sockfd);

                    else
                    {
                        struct sock_ctx *new_ctx = serv->alloc(serv, ntohs(curr_ctx->addr_in.sin_port));
                        if(new_ctx == NULL)
                            socClose(client_sockfd);
                        else
                        {
                            fds[serv->nfds].fd = client_sockfd;
                            fds[serv->nfds].events = POLLIN;
                            fds[serv->nfds].revents = 0;

                            int new_idx = serv->nfds;
                            serv->nfds++;
                            curr_ctx->n++;

                            new_ctx->type = SOCK_CLIENT;
                            new_ctx->sockfd = client_sockfd;
                            new_ctx->serv = curr_ctx;
                            new_ctx->i = new_idx;
                            new_ctx->n = 0;
                            new_ctx->should_close = false;

                            serv->ctx_ptrs[new_idx] = new_ctx;

                            if(serv->accept_cb(new_ctx) == -1)
                                server_close_ctx(serv, new_ctx);

                            memcpy(&new_ctx->addr_in, &saddr, sizeof(struct sockaddr_in));
                        }
                    }
                }
                else
                {
                    if(serv->data_cb(curr_ctx) == -1)
                        server_close_ctx(serv, curr_ctx);
                }
            }
        }

        if(server_should_exit(serv))
            goto abort_connections;

        if(serv->compact_needed)
            compact(serv);
    }

    // Clean up.
    for(unsigned int i = 0; i < serv->nfds; i++)
    {
        if(fds[i].fd != -1)
            socClose(fds[i].fd);
    }

    serv->running = false;
    svcClearEvent(serv->started_event);
    return;

abort_connections:
    server_kill_connections(serv);
    serv->running = false;
    svcClearEvent(serv->started_event);
    svcSignalEvent(serv->shall_terminate_event);
}

void server_set_should_close_all(struct sock_server *serv)
{
    nfds_t nfds = serv->nfds;

    for(unsigned int i = 0; i < nfds; i++)
        serv->ctx_ptrs[i]->should_close = true;
}

void server_kill_connections(struct sock_server *serv)
{
    struct pollfd *fds = serv->poll_fds;
    nfds_t nfds = serv->nfds;

    for(unsigned int i = 0; i < nfds; i++)
    {
        if(fds[i].fd == -1)
            continue;

        struct linger linger;
        linger.l_onoff = 1;
        linger.l_linger = 0;

        socSetsockopt(fds[i].fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(struct linger));
        socClose(fds[i].fd);
        fds[i].fd = -1;

        if(serv->ctx_ptrs[i] != NULL)
            serv->ctx_ptrs[i]->should_close = true;
    }
}

void server_finalize(struct sock_server *serv)
{
    for(nfds_t i = 0; i < MAX_CTXS; i++)
    {
        if(serv->ctx_ptrs[i] != NULL)
            server_close_ctx(serv, serv->ctx_ptrs[i]);
    }

    miniSocExit();

    svcClearEvent(serv->shall_terminate_event);
    svcCloseHandle(serv->shall_terminate_event);
    svcClearEvent(serv->started_event);
    svcCloseHandle(serv->started_event);
}
