/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2019 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#pragma once

#include <sys/socket.h>
#include <3ds/types.h>
#include <3ds/svc.h>
#include <3ds/srv.h>
#include <3ds/services/soc.h>
#include <poll.h>

#define _REENT_ONLY
#include <errno.h>

#define SYNC_ERROR ENODEV

extern Handle SOCU_handle;
extern Handle socMemhandle;

extern bool miniSocEnabled;

Result miniSocInit();
Result miniSocExit(void);

s32 _net_convert_error(s32 sock_retval);

int socSocket(int domain, int type, int protocol);
int socBind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int socListen(int sockfd, int max_connections);
int socAccept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int socConnect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int socPoll(struct pollfd *fds, nfds_t nfds, int timeout);
int socSetsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int socClose(int sockfd);

ssize_t soc_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t soc_send(int sockfd, const void *buf, size_t len, int flags);

// actually provided by ctrulib
ssize_t soc_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t soc_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
