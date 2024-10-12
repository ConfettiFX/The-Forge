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

#ifndef NETWORK_H
#define NETWORK_H

#include "../../Application/Config.h"

#ifndef __cplusplus
#include <stdbool.h>
#endif

#define SOCKET_ADDR_SIZE     128
#define SOCKET_HOST_MAX_SIZE 64
#define SOCKET_HOST_ANY      NULL
#define SOCKET_HOST_ANY6     ((const char*)0x1)
#define SOCKET_INVALID       (~((Socket)0))

#ifdef __cplusplus
extern "C"
{
#endif
    /// Platform independent network address
    typedef struct ALIGNAS(64) SocketAddr
    {
        char mStorage[SOCKET_ADDR_SIZE];
    } SocketAddr;

/// Platform-independent socket for sending/receiving messages over the network
#ifdef _WIN32
    typedef uint64_t Socket;
#else
typedef int Socket;
#endif

    // Setup (Windows only, no-op elsewhere)
    bool initNetwork(void);
    void exitNetwork(void);

    // Create a new socket address from host/port
    bool    socketAddrFromHostPort(SocketAddr* addr, const char* host, uint16_t port);
    bool    socketAddrFromHostnamePort(SocketAddr* addr, const char* hostname, uint16_t port);
    // Get host/port from given socket address
    void    socketAddrToHostPort(const SocketAddr* addr, char* host, size_t hostSize, uint16_t* port);
    /// Create a new TCP server socket
    bool    socketCreateServer(Socket* sock, const SocketAddr* addr, int backlog);
    /// Create a new TCP client socket
    bool    socketCreateClient(Socket* sock, const SocketAddr* addr);
    // Destroy a socket
    bool    socketDestroy(Socket* sock);
    /// Accept a new connection on the given socket
    bool    socketAccept(Socket* sock, Socket* conn, SocketAddr* connAddr);
    /// Send data to the connected socket
    ssize_t socketSend(Socket* sock, const void* data, size_t dataSize);
    /// Receive data from the connected socket
    ssize_t socketRecv(Socket* sock, void* data, size_t dataSize);

#ifdef __cplusplus
}
#endif

#endif
