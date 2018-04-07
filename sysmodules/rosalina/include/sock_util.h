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

/* File mainly written by Stary */

#pragma once
#include <3ds/types.h>
#include <poll.h>
#include <netinet/in.h>

#define MAX_PORTS 3
#define MAX_CTXS  (2 * MAX_PORTS)

struct sock_server;
struct sock_ctx;

typedef int (*sock_accept_cb)(struct sock_ctx *client_ctx);
typedef int (*sock_data_cb)(struct sock_ctx *client_ctx);
typedef int (*sock_close_cb)(struct sock_ctx *client_ctx);

typedef struct sock_ctx* (*sock_alloc_func)(struct sock_server *serv, u16 port);
typedef void (*sock_free_func)(struct sock_server *serv, struct sock_ctx *ctx);

typedef enum socket_type
{
    SOCK_NONE,
    SOCK_SERVER,
    SOCK_CLIENT
} socket_type;

typedef struct sock_ctx
{
    enum socket_type type;
    bool should_close;
    int sockfd;
    struct sockaddr_in addr_in;
    struct sock_ctx *serv;
    int n;
    int i;
} sock_ctx;

typedef struct sock_server
{
    // params
    u32 host;
    int clients_per_server;

    // poll stuff
    struct pollfd poll_fds[MAX_CTXS];
    struct sock_ctx serv_ctxs[MAX_PORTS];
    struct sock_ctx *ctx_ptrs[MAX_CTXS];

    nfds_t nfds;
    bool running;
    Handle started_event;
    bool compact_needed;

    // callbacks
    sock_accept_cb accept_cb;
    sock_data_cb data_cb;
    sock_close_cb close_cb;

    sock_alloc_func alloc;
    sock_free_func free;

    Handle shall_terminate_event;
} sock_server;

Result server_init(struct sock_server *serv);
void server_bind(struct sock_server *serv, u16 port);
void server_run(struct sock_server *serv);
void server_finalize(struct sock_server *serv);
