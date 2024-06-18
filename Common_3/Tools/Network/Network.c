/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "Network.h"

#include <string.h>

#include "../../Utilities/Interfaces/ILog.h"

#define ENETWORKUNINITALIZED INT_MAX

#if defined(_WINDOWS) || defined(XBOX)
/// Windows sockets (WinSock2)

#include <WS2tcpip.h> // inet_pton/inet_ntop
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")

typedef int socklen_t;

#define INVALID_SOCKET_HANDLE INVALID_SOCKET

#define close                 closesocket

#define socketLastError       WSAGetLastError

#else
/// Unix BSD-style sockets

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define INVALID_SOCKET_HANDLE -1

#define socketLastError()     errno

#endif

// WinSock2 requires calling init/exit functions
#if defined(_WINDOWS) || defined(XBOX)

static bool gInitialized = false;

bool initNetwork(void)
{
    if (gInitialized)
        return true;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        LOGF(eERROR, "Failed to initialize Network");
        return false;
    }

    return true;
}

void exitNetwork(void)
{
    if (gInitialized)
        gInitialized = WSACleanup() == 0;
}

#elif defined(NX64)

extern bool initNetwork(void);
extern void exitNetwork(void);

#else

bool initNetwork(void) { return true; }
void exitNetwork(void) {}

#endif

// Some platforms require additional setup in order for sockets to behave similarly
// across platforms. These functions allow for this while minimizing code duplication.
#if defined(__ORBIS__) || defined(__PROSPERO__)
extern void platformSocketSetupClient(Socket s);
extern bool platformSocketOnErrorConnect(const SocketAddr* addr);
#else
static void platformSocketSetupClient(Socket s) { (void)s; }
static bool platformSocketOnErrorConnect(const SocketAddr* addr)
{
    (void)addr;
    return false;
}
#endif

// Some consoles don't support IPV6 so we define fake structs here since they will never be used.
#if defined(NX64) || defined(PROSPERO) || defined(ORBIS)

struct in6_addr
{
    unsigned char s6_addr[16];
};

struct sockaddr_in6
{
    uint16_t        sin6_family;
    uint16_t        sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

static const struct in6_addr in6addr_any;

#endif

// NOTE: These macros have function-like names but SCREAMING_SNAKE_CASE makes the names too long
#define addrPSaFamily(addr) (&((struct sockaddr*)addr)->sa_family)
#define addrIsIPV6(addr)    (*addrPSaFamily(addr) == AF_INET6)

#define addrLen(addr)       (addrIsIPV6(addr) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in))

#define addrPSinPort(addr)  (addrIsIPV6(addr) ? &((struct sockaddr_in6*)addr)->sin6_port : &((struct sockaddr_in*)addr)->sin_port)

#define addrPSinAddr(addr) \
    (addrIsIPV6(addr) ? (void*)&((struct sockaddr_in6*)addr)->sin6_addr : (void*)&((struct sockaddr_in*)addr)->sin_addr)

static const char* socketLastErrorMessage(void)
{
#if defined(_WINDOWS) || defined(XBOX)
    static __declspec(thread) char errorBuf[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL,
                   (DWORD)socketLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorBuf, sizeof(errorBuf), NULL);
    return errorBuf;
#else
    return strerror(socketLastError());
#endif
}

static bool socketError(const char* msg, const SocketAddr* addr)
{
    if (addr)
    {
        char     host[SOCKET_HOST_MAX_SIZE];
        uint16_t port = 0;
        socketAddrToHostPort(addr, host, sizeof(host), &port);
        LOGF(eERROR, "%s `%s:%u`: %s", msg, host, (uint32_t)port, socketLastErrorMessage());
    }
    else
    {
        LOGF(eERROR, "%s: %s", msg, socketLastErrorMessage());
    }
    return false;
}

bool socketAddrFromHostPort(SocketAddr* addr, const char* host, uint16_t port)
{
    ASSERT(addr);
    memset(addr, 0, sizeof(SocketAddr));

    // IPV6 addresses contain `:`, and IPV4 addresses do not
    // If no host is provided, then we default to IPV4
    int     hasColon = host && host > SOCKET_HOST_ANY6 && memchr(host, ':', strnlen(host, SOCKET_HOST_MAX_SIZE));
    int16_t family = (host == SOCKET_HOST_ANY6 || hasColon) ? AF_INET6 : AF_INET;

    *addrPSaFamily(addr) = family;
    *addrPSinPort(addr) = htons(port);

    // IPV6 requires we initialize wildcard addresses with `in6addr_any`
    if (family == AF_INET6 && host == SOCKET_HOST_ANY6)
    {
        memcpy(addrPSinAddr(addr), &in6addr_any, sizeof(in6addr_any));
    }
    // Since we memset to 0, we only need to set an IPV4 address if one is provided
    else if (host && host[0])
    {
        const char* realHost = (strncmp(host, "localhost", 9) == 0) ? "127.0.0.1" : host;
        if (!inet_pton(family, realHost, addrPSinAddr(addr)))
        {
            LOGF(eERROR, "Failed to create socket address from `%s:%u`: %s", host, (uint32_t)port, socketLastErrorMessage());
            return false;
        }
    }
    return true;
}

#ifdef FORGE_TOOLS // Name resolution is only supported when running the UIRemoteControl tool
bool socketAddrFromHostnamePort(SocketAddr* addr, const char* hostname, uint16_t port)
{
    ASSERT(addr);
    memset(addr, 0, sizeof(SocketAddr));

    bool             success = true;
    struct addrinfo* result;

    int family = AF_INET;

    if (hostname == 0)
    {
        // Just set the family and port
        *addrPSaFamily(addr) = (int16_t)family;
        *addrPSinPort(addr) = htons(port);
        return true;
    }

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char portStr[12] = { 0 };
    snprintf(portStr, 12, "%d", port);
    success = (getaddrinfo(hostname, portStr, &hints, &result) == 0);

    if (!success)
    {
        LOGF(eERROR, "Failed to create socket address from `%s:%s`: %s", hostname, portStr, socketLastErrorMessage());
    }
    else
    {
        family = result->ai_family;

        *addrPSaFamily(addr) = (int16_t)family;
        *addrPSinPort(addr) = ((struct sockaddr_in*)result->ai_addr)->sin_port;

        // IPV6 requires we initialize wildcard addresses with `in6addr_any`
        if (family == AF_INET6 && hostname == SOCKET_HOST_ANY6)
        {
            memcpy(addrPSinAddr(addr), &in6addr_any, sizeof(in6addr_any));
        }

        memcpy(addrPSinAddr(addr), &((struct sockaddr_in*)result->ai_addr)->sin_addr, result->ai_addrlen);
    }

    return success;
}
#endif // FORGE_TOOLS

void socketAddrToHostPort(const SocketAddr* addr, char* host, size_t hostSize, uint16_t* port)
{
    ASSERT(addr);

    if (host)
        inet_ntop(*addrPSaFamily(addr), addrPSinAddr(addr), host, (socklen_t)hostSize);

    if (port)
        *port = ntohs(*addrPSinPort(addr));
}

bool socketCreateServer(Socket* sock, const SocketAddr* addr, int backlog)
{
    ASSERT(sock);
    *sock = SOCKET_INVALID;

    int    family = *addrPSaFamily(addr);
    Socket s = socket(family, SOCK_STREAM, 0);

    if (s == INVALID_SOCKET_HANDLE)
        return socketError("Failed to create socket", NULL);

    // This is just a convenience option so we don't care if it fails
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const void*)&yes, sizeof(yes));

    socklen_t len = addrLen(addr);
    if (bind(s, (struct sockaddr*)addr->mStorage, len) != 0)
        return socketError("Failed to bind to socket address", addr);

    if (listen(s, backlog) != 0)
        return socketError("Failed to listen on socket at", addr);

    *sock = s;
    return true;
}

bool socketCreateClient(Socket* sock, const SocketAddr* addr)
{
    ASSERT(sock);
    *sock = SOCKET_INVALID;

    int    family = *addrPSaFamily(addr);
    Socket s = socket(family, SOCK_STREAM, 0);

    if (s == INVALID_SOCKET_HANDLE)
        return socketError("Failed to create socket", NULL);

    platformSocketSetupClient(s);

    socklen_t len = addrLen(addr);
    if (connect(s, (struct sockaddr*)addr->mStorage, len) != 0)
    {
        if (platformSocketOnErrorConnect(addr))
        {
            return false;
        }
        else
        {
            return socketError("Failed to connect socket", addr);
        }
    }

    *sock = s;
    return true;
}

bool socketDestroy(Socket* sock)
{
    ASSERT(sock);
    ASSERT(*sock != SOCKET_INVALID && "Socket has not been initialized");
#ifndef _WIN32
    shutdown(*sock, SHUT_RDWR);
#endif
    if (close(*sock) != 0)
        return socketError("Failed to destroy socket", NULL);

    *sock = SOCKET_INVALID;
    return true;
}

bool socketAccept(Socket* sock, Socket* conn, SocketAddr* connAddr)
{
    ASSERT(sock && conn && connAddr);
    ASSERT(*sock != SOCKET_INVALID && "Socket has not been initialized");
    *conn = SOCKET_INVALID;

    socklen_t len = sizeof(struct sockaddr_in6);
    Socket    s = accept(*sock, (struct sockaddr*)connAddr->mStorage, &len);

    if (s == INVALID_SOCKET_HANDLE)
    {
        // You can make `socketAccept` exit early by closing the socket, which is not an error
        // AFAIK this is the only way to interrupt a blocking `accept` without platform-specific stuff.
#if defined(__APPLE__) || defined(NX64)
        int inter = ECONNABORTED;
#elif defined(ORBIS) || defined(PROSPERO)
        int inter = ENETINTR;
#elif defined(_WINDOWS) || defined(XBOX)
        int inter = WSAEINTR;
#else
        int inter = EINTR;
#endif
        if (socketLastError() != inter)
            LOGF(eERROR, "Failed to accept new connection: %s", socketLastErrorMessage());
        return false;
    }

    *conn = s;
    return true;
}

ssize_t socketSend(Socket* sock, const void* data, size_t dataSize)
{
    ASSERT(sock && data);
    ASSERT(*sock != SOCKET_INVALID && "Socket has not been initialized");

    ssize_t r = send(*sock, data, (int)dataSize, 0);
    if (r < 0)
        socketError("Failed to send data to socket", NULL);

    return r;
}

ssize_t socketRecv(Socket* sock, void* data, size_t dataSize)
{
    ASSERT(sock && data);
    ASSERT(*sock != SOCKET_INVALID && "Socket has not been initialized");

    ssize_t r = recv(*sock, data, (int)dataSize, 0);
    if (r < 0)
        socketError("Failed to receive data from socket", NULL);

    return r;
}
