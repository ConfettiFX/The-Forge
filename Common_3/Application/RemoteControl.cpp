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

#include "../Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../Utilities/Interfaces/ILog.h"
#include "../Utilities/Interfaces/IThread.h"
#include "Interfaces/IUI.h"

#include "../Tools/Network/Network.h"
#include "../Utilities/Threading/Atomics.h"

#ifdef ENABLE_FORGE_REMOTE_UI
/****************************************************************************/
// MARK: - Message/Packet Structs for Networking
/****************************************************************************/

enum RemoteControlMessageType : uint8_t
{
    REMOTE_MESSAGE_INVALID,
    REMOTE_MESSAGE_DRAW,
    REMOTE_MESSAGE_INPUT,
    REMOTE_MESSAGE_TEXTURE,
    REMOTE_MESSAGE_PING,
    REMOTE_MESSAGE_DISCONNECT,
};

typedef struct RemoteCommandHeader
{
    uint32_t                 mSize = 0;
    RemoteControlMessageType mType = REMOTE_MESSAGE_INVALID;
    uint8_t                  mPadding[3] = { 0, 0, 0 };
} RemoteCommandHeader;

typedef struct alignas(8) RemoteCommandUserInterfaceDrawCommand
{
    UserInterfaceDrawElement mElement;
    uint32_t                 mPadding;
} RemoteCommandUserInterfaceDrawElement;

typedef struct alignas(8) RemoteCommandUserInterfaceDrawData
{
    RemoteCommandHeader mHeader = RemoteCommandHeader{ sizeof(RemoteCommandUserInterfaceDrawData), REMOTE_MESSAGE_DRAW };
    uint32_t            mVertexCount;
    uint32_t            mIndexCount;
    uint32_t            mVertexSize;
    uint32_t            mIndexSize;
    float2              mDisplayPos;
    float2              mDisplaySize;
    uint32_t            mNumDrawCommands;
    uint32_t            mPadding;
} RemoteCommandUserInterfaceDrawData;

typedef struct alignas(8) RemoteCommandUserInterfaceInput
{
    RemoteCommandHeader mHeader = RemoteCommandHeader{ sizeof(RemoteCommandUserInterfaceInput), REMOTE_MESSAGE_INPUT };
    uint32_t            mNumInputs;
    uint32_t            mPadding;
} RemoteCommandUserInterfaceInput;

typedef struct alignas(8) RemoteCommandUserInterfaceInputData
{
    uint32_t mActionId;
    bool     mButtonPress;
    float2   mMousePos;
    float2   mStick;
    bool     mSkipMouse;
    uint32_t mPadding;
} RemoteCommandUserInterfaceInputData;

typedef struct alignas(8) RemoteCommandUserInterfaceTexture
{
    RemoteCommandHeader mHeader = RemoteCommandHeader{ sizeof(RemoteCommandUserInterfaceTexture), REMOTE_MESSAGE_TEXTURE };
    uint32_t            mFormat;
    uint32_t            mWidth;
    uint32_t            mHeight;
    uint32_t            mTextureSize;
    uint64_t            mTextureId;
} RemoteCommandUserInterfaceTextureCommand;

/****************************************************************************/
// MARK: - Circular Buffer for server/client
/****************************************************************************/

// 1 MB
#define InputBufferSize (1 * (1 << 20))
// 16 MB
#define DrawBufferSize  (16 * (1 << 20))

struct CircularBuffer
{
    unsigned char* mBuffer1;
    unsigned char* mBuffer2;

    tfrg_atomic64_t mActiveBuffer;
    tfrg_atomic64_t mIsBufferReady[2];
    tfrg_atomic64_t mIsBufferInUse[2];

    size_t mSize;
};

void init_circular_buffer(CircularBuffer* circularBuffer, size_t size)
{
    circularBuffer->mSize = size;
    circularBuffer->mBuffer1 = (unsigned char*)tf_malloc(size);
    circularBuffer->mBuffer2 = (unsigned char*)tf_malloc(size);
    memset(circularBuffer->mBuffer1, 0, size);
    memset(circularBuffer->mBuffer2, 0, size);

    tfrg_atomic64_store_relaxed(&circularBuffer->mActiveBuffer, 0);
    tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferReady[0], 0);
    tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferReady[1], 0);
    tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferInUse[0], 0);
    tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferInUse[1], 0);
}

void remove_circular_buffer(CircularBuffer* circularBuffer)
{
    tf_free(circularBuffer->mBuffer1);
    tf_free(circularBuffer->mBuffer2);
}

unsigned char* get_write_buffer(CircularBuffer* circularBuffer)
{
    return (tfrg_atomic64_load_relaxed(&circularBuffer->mActiveBuffer) == 0) ? circularBuffer->mBuffer1 : circularBuffer->mBuffer2;
}

bool swap_buffers(CircularBuffer* circularBuffer)
{
    uint64_t currentBuffer = tfrg_atomic64_load_relaxed(&circularBuffer->mActiveBuffer);
    uint64_t nextBuffer = 1 - currentBuffer;

    if (!tfrg_atomic64_load_relaxed(&circularBuffer->mIsBufferReady[nextBuffer]))
    {
        // Mark current buffer as ready and swap if next buffer is not in use
        tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferReady[currentBuffer], 1);
        tfrg_atomic64_store_relaxed(&circularBuffer->mActiveBuffer, nextBuffer);

        return true;
    }
    return false;
}

unsigned char* get_read_buffer(CircularBuffer* circularBuffer)
{
    // Return the buffer that is not currently active and ready
    uint64_t readBufferIndex = 1 - tfrg_atomic64_load_relaxed(&circularBuffer->mActiveBuffer);
    if (tfrg_atomic64_load_relaxed(&circularBuffer->mIsBufferReady[readBufferIndex]))
    {
        tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferInUse[readBufferIndex], 1);
        return (readBufferIndex == 0) ? circularBuffer->mBuffer1 : circularBuffer->mBuffer2;
    }
    return nullptr; // No buffer ready to read
}

void mark_buffer_processed(CircularBuffer* circularBuffer)
{
    // Mark the read buffer as processed
    uint64_t readBufferIndex = 1 - tfrg_atomic64_load_relaxed(&circularBuffer->mActiveBuffer);
    if (readBufferIndex == 0)
    {
        memset(circularBuffer->mBuffer1, 0, circularBuffer->mSize);
    }
    else
    {
        memset(circularBuffer->mBuffer2, 0, circularBuffer->mSize);
    }
    tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferReady[readBufferIndex], 0);
    tfrg_atomic64_store_relaxed(&circularBuffer->mIsBufferInUse[readBufferIndex], 0);
}

/****************************************************************************/
// MARK: - Common
/****************************************************************************/

static size_t bufferedSend(Socket* socket, unsigned char* data, size_t size)
{
    size_t remainingSize = size;
    while (remainingSize > 0)
    {
        ssize_t receivedSize = socketSend(socket, data + size - remainingSize, remainingSize);
        if (receivedSize > 0)
        {
            remainingSize -= receivedSize;
        }
        else if (receivedSize == 0)
        {
            return size - remainingSize;
        }
        else
        {
            LOGF(eWARNING, "Socket connection was disconnected");
            socketDestroy(socket);
            return 0;
        }
    }

    return size;
}

static size_t bufferedReceive(Socket* socket, unsigned char* data, size_t size)
{
    size_t remainingSize = size;
    while (remainingSize > 0)
    {
        ssize_t receivedSize = socketRecv(socket, data + size - remainingSize, remainingSize);
        if (receivedSize > 0)
        {
            remainingSize -= receivedSize;
        }
        else if (receivedSize == 0)
        {
            return size - remainingSize;
        }
        else
        {
            LOGF(eWARNING, "Socket connection was disconnected");
            socketDestroy(socket);
            return 0;
        }
    }

    return size;
}

static bool sendPing(Socket* socket)
{
    RemoteCommandHeader remoteCommandHeader = {};
    remoteCommandHeader.mSize = sizeof(remoteCommandHeader);
    remoteCommandHeader.mType = REMOTE_MESSAGE_PING;

    return bufferedSend(socket, (unsigned char*)&remoteCommandHeader, sizeof(remoteCommandHeader));
}

static bool sendDisconnect(Socket* socket)
{
    RemoteCommandHeader remoteCommandHeader = {};
    remoteCommandHeader.mSize = sizeof(remoteCommandHeader);
    remoteCommandHeader.mType = REMOTE_MESSAGE_DISCONNECT;

    return bufferedSend(socket, (unsigned char*)&remoteCommandHeader, sizeof(remoteCommandHeader));
}

struct TextureNode
{
    uint64_t key;
    Texture* value;
};

struct RemoteAppServer
{
    CircularBuffer                       mAwaitingSendDrawData;
    CircularBuffer                       mAwaitingSendTextureData;
    CircularBuffer                       mReceivedInput;
    RemoteCommandUserInterfaceInputData* pLastReceivedInputData;

    time_t       mLastPingTime;
    ThreadHandle mServerThread = {};
    Socket       mServerListenSocket;
    Socket       mServerConnectionSocket;
    bool         mDisconnect = false;
    bool         mShouldSendFontTexture = false;
};

struct RemoteControlClient
{
    RemoteCommandUserInterfaceInputData* inputs;
    CircularBuffer                       mAwaitingSendInput;
    CircularBuffer                       mReceivedDrawData;
    CircularBuffer                       mReceivedTextureData;
    UserInterfaceDrawData                mLastReceivedDrawData;
    TextureNode*                         mTextureHashmap;

    time_t       mLastPingTime;
    ThreadHandle mClientThread;
    Socket       mClientConnectionSocket;
    bool         mDisconnect = false;
};

static RemoteAppServer* pRemoteAppServer = NULL;
#ifdef FORGE_TOOLS
static RemoteControlClient* pRemoteControlClient = NULL;
#endif // FORGE_TOOLS

// Timeout in seconds when server does not receive ping
#define TIMEOUT_SECONDS 10

/// Convert UserInterfaceDrawData to a remote control packet to send to the connected client
static unsigned char* packUserInterfaceDrawData(UserInterfaceDrawData* pDrawData, unsigned char* packedData)
{
    size_t packetSize = sizeof(RemoteCommandUserInterfaceDrawData);
    packetSize += pDrawData->mNumDrawCommands * sizeof(RemoteCommandUserInterfaceDrawCommand);

    size_t vertexSizeAligned = ((size_t)pDrawData->mVertexCount * pDrawData->mVertexSize / 8 + 1) * 8;
    packetSize += vertexSizeAligned;
    size_t indexSizeAligned = ((size_t)pDrawData->mIndexCount * pDrawData->mIndexSize / 8 + 1) * 8;
    packetSize += indexSizeAligned;

    size_t offset = 0;

    RemoteCommandUserInterfaceDrawData drawData = {};
    drawData.mHeader.mSize = (uint32_t)packetSize;
    drawData.mVertexCount = pDrawData->mVertexCount;
    drawData.mIndexCount = pDrawData->mIndexCount;
    drawData.mVertexSize = pDrawData->mVertexSize;
    drawData.mIndexSize = pDrawData->mIndexSize;
    drawData.mDisplayPos = pDrawData->mDisplayPos;
    drawData.mDisplaySize = pDrawData->mDisplaySize;
    drawData.mNumDrawCommands = pDrawData->mNumDrawCommands;

    memcpy(packedData + offset, &drawData, sizeof(RemoteCommandUserInterfaceDrawData));
    offset += sizeof(RemoteCommandUserInterfaceDrawData);

    for (uint32_t i = 0; i < pDrawData->mNumDrawCommands; i++)
    {
        RemoteCommandUserInterfaceDrawCommand drawCommand = {};
        drawCommand.mElement = pDrawData->mDrawCommands[i];

        memcpy(packedData + offset, &drawCommand, sizeof(RemoteCommandUserInterfaceDrawCommand));
        offset += sizeof(RemoteCommandUserInterfaceDrawCommand);
    }

    memcpy(packedData + offset, pDrawData->mVertexBufferData, (size_t)pDrawData->mVertexCount * pDrawData->mVertexSize);
    offset += vertexSizeAligned;

    memcpy(packedData + offset, pDrawData->mIndexBufferData, (size_t)pDrawData->mIndexCount * pDrawData->mIndexSize);
    offset += indexSizeAligned;

    return packedData;
}

/****************************************************************************/
// MARK: - Remote App Server
/****************************************************************************/

static bool serverSend(Socket* socket)
{
    if (pRemoteAppServer->mDisconnect)
    {
        sendDisconnect(socket);
        // Ensure the client receives the disconnect and stop gracefully.
        threadSleep(100);
        return false;
    }

    bool sendSucceed = true;

    unsigned char* drawDataToSend = get_read_buffer(&pRemoteAppServer->mAwaitingSendDrawData);
    if (drawDataToSend)
    {
        sendSucceed =
            sendSucceed && (bufferedSend(socket, drawDataToSend, ((RemoteCommandUserInterfaceDrawData*)drawDataToSend)->mHeader.mSize) > 0);
        mark_buffer_processed(&pRemoteAppServer->mAwaitingSendDrawData);
    }

    if (!sendSucceed)
        return false;

    unsigned char* textureDataToSend = get_read_buffer(&pRemoteAppServer->mAwaitingSendTextureData);
    if (textureDataToSend)
    {
        sendSucceed &=
            (bufferedSend(socket, textureDataToSend, ((RemoteCommandUserInterfaceTexture*)textureDataToSend)->mHeader.mSize) > 0);
        mark_buffer_processed(&pRemoteAppServer->mAwaitingSendTextureData);
    }

    if (!sendSucceed)
        return false;

    sendSucceed = sendPing(socket);
    return sendSucceed;
}

static void disconnectFromClient()
{
    if (pRemoteAppServer->mServerConnectionSocket != SOCKET_INVALID)
    {
        socketDestroy(&pRemoteAppServer->mServerConnectionSocket);
    }

    LOGF(LogLevel::eINFO, "Client disconnected.");

    if (!uiIsRenderingEnabled())
    {
        uiToggleRendering(true);
    }
}

static void serverReceive(Socket* socket)
{
    bool pingReceived = false;

    while (!pingReceived)
    {
        RemoteCommandHeader remoteCommandHeader = {};
        bool                receivedHeader = bufferedReceive(socket, (unsigned char*)&remoteCommandHeader, sizeof(RemoteCommandHeader));

        if (receivedHeader)
        {
            switch (remoteCommandHeader.mType)
            {
            case REMOTE_MESSAGE_INPUT:
            {
                RemoteCommandUserInterfaceInput inputData = {};
                inputData.mHeader = remoteCommandHeader;

                bool isDataReceived = bufferedReceive(socket, ((unsigned char*)&inputData) + sizeof(RemoteCommandHeader),
                                                      sizeof(RemoteCommandUserInterfaceInput) - sizeof(RemoteCommandHeader));
                if (isDataReceived)
                {
                    unsigned char* oldInputs = get_write_buffer(&pRemoteAppServer->mReceivedInput);
                    uint32_t*      oldNumInputs = &((RemoteCommandUserInterfaceInput*)oldInputs)->mNumInputs;
                    isDataReceived = bufferedReceive(
                        socket,
                        oldInputs + sizeof(RemoteCommandUserInterfaceInput) + *oldNumInputs * sizeof(RemoteCommandUserInterfaceInputData),
                        remoteCommandHeader.mSize - sizeof(RemoteCommandUserInterfaceInput) - sizeof(RemoteCommandHeader));
                    if (isDataReceived)
                    {
                        *oldNumInputs += inputData.mNumInputs;
                    }
                    swap_buffers(&pRemoteAppServer->mReceivedInput);
                }
            }
            break;
            case REMOTE_MESSAGE_PING:
            {
                pRemoteAppServer->mLastPingTime = time(0);
                pingReceived = true;
            }
            break;
            case REMOTE_MESSAGE_DISCONNECT:
            {
                disconnectFromClient();
                pingReceived = true;
            }
            break;
            default:
                break;
            }
        }
        else
        {
            break;
        }
    }
}

static void server(void* pData)
{
    UNREF_PARAM(pData);
    while (pRemoteAppServer->mServerListenSocket && !pRemoteAppServer->mDisconnect)
    {
        SocketAddr addr = {};
        socketAccept(&pRemoteAppServer->mServerListenSocket, &pRemoteAppServer->mServerConnectionSocket, &addr);
        pRemoteAppServer->mLastPingTime = time(0);
        pRemoteAppServer->mShouldSendFontTexture = true;

        while (pRemoteAppServer->mServerConnectionSocket != SOCKET_INVALID)
        {
            bool sendSucceed = serverSend(&pRemoteAppServer->mServerConnectionSocket);

            if (sendSucceed)
            {
                serverReceive(&pRemoteAppServer->mServerConnectionSocket);
            }

            // Timeout in TIMEOUT_SECONDS seconds
            if (time(0) - pRemoteAppServer->mLastPingTime > TIMEOUT_SECONDS || !sendSucceed)
            {
                disconnectFromClient();
                break;
            }

            threadSleep(10);
        }

        if (pRemoteAppServer->mServerConnectionSocket != SOCKET_INVALID)
        {
            disconnectFromClient();
        }

        threadSleep(10);
    }

    if (pRemoteAppServer->mServerListenSocket != SOCKET_INVALID)
    {
        socketDestroy(&pRemoteAppServer->mServerListenSocket);
    }
}

/// Initialize remote control server.
/// The server application sends the UI draw commands to connected client.
/// Clients send the input data.
void initRemoteAppServer(uint16_t port)
{
    if (pRemoteAppServer != NULL)
    {
        return;
    }

    pRemoteAppServer = (RemoteAppServer*)tf_malloc(sizeof(RemoteAppServer));
    *pRemoteAppServer = {};

    init_circular_buffer(&pRemoteAppServer->mAwaitingSendDrawData, DrawBufferSize);
    init_circular_buffer(&pRemoteAppServer->mAwaitingSendTextureData, DrawBufferSize);
    init_circular_buffer(&pRemoteAppServer->mReceivedInput, InputBufferSize);

    SocketAddr addr = {};
    socketAddrFromHostPort(&addr, 0, port);
    socketCreateServer(&pRemoteAppServer->mServerListenSocket, &addr, 0);

    ThreadDesc threadDesc = {};
    threadDesc.pFunc = server;
    threadDesc.pData = NULL;
    strncpy(threadDesc.mThreadName, "RemoteAppServerThread", sizeof(threadDesc.mThreadName));
    initThread(&threadDesc, &pRemoteAppServer->mServerThread);
}

void remoteServerDisconnect()
{
    if (pRemoteAppServer && !pRemoteAppServer->mDisconnect)
    {
        pRemoteAppServer->mDisconnect = true;

        if (pRemoteAppServer->mServerConnectionSocket == SOCKET_INVALID)
        {
            // There's no active client connected so we should just destroy the socket here
            // since we don't have to send a disconnect message anymore
            socketDestroy(&pRemoteAppServer->mServerListenSocket);
        }
        // If there is currently an active client connected we need to send a disconnect message first,
        // so we leave the serverListenSocket open. It will get destroyed after sending the disconnect message

        if (pRemoteAppServer->mServerThread)
        {
            joinThread(pRemoteAppServer->mServerThread);
        }
    }
}

/// Free all the memory allocated for the server.
/// Disconnect from all connected sockets.
void exitRemoteAppServer()
{
    remoteServerDisconnect();

    ASSERT(pRemoteAppServer->mServerListenSocket == SOCKET_INVALID && "Server Listen Socket is still active");

    // Free all awaiting draw data
    remove_circular_buffer(&pRemoteAppServer->mAwaitingSendDrawData);
    remove_circular_buffer(&pRemoteAppServer->mAwaitingSendTextureData);

    // Free all received inputs
    remove_circular_buffer(&pRemoteAppServer->mReceivedInput);
    arrfree(pRemoteAppServer->pLastReceivedInputData);

    // Free remote control server
    tf_free(pRemoteAppServer);
    pRemoteAppServer = NULL;
}

/// Process received input data from the client by calling the UI system.
void remoteAppReceiveInputData()
{
    if (!pRemoteAppServer || pRemoteAppServer->mServerConnectionSocket == SOCKET_INVALID)
    {
        return;
    }

    unsigned char* receivedInput = get_read_buffer(&pRemoteAppServer->mReceivedInput);
    arrsetlen(pRemoteAppServer->pLastReceivedInputData, 0);

    if (receivedInput)
    {
        uint32_t numReceivedInputs = ((RemoteCommandUserInterfaceInput*)receivedInput)->mNumInputs;
        for (uint32_t i = 0; i < numReceivedInputs; i++)
        {
            RemoteCommandUserInterfaceInputData* input =
                (RemoteCommandUserInterfaceInputData*)(receivedInput + sizeof(RemoteCommandUserInterfaceInput) +
                                                       sizeof(RemoteCommandUserInterfaceInputData) * i);
            arrpush(pRemoteAppServer->pLastReceivedInputData, *input);
        }
        mark_buffer_processed(&pRemoteAppServer->mReceivedInput);
    }

    for (uint32_t i = 0; i < arrlen(pRemoteAppServer->pLastReceivedInputData); i++)
    {
        RemoteCommandUserInterfaceInputData* input = &pRemoteAppServer->pLastReceivedInputData[i];
        uiOnInput(input->mActionId, input->mButtonPress, input->mSkipMouse ? NULL : &input->mMousePos, &input->mStick);
    }
}

/// Send a given UserInterfaceDrawData to connected client.
void remoteAppSendDrawData(UserInterfaceDrawData* pDrawData)
{
    if (!pRemoteAppServer || pRemoteAppServer->mServerConnectionSocket == SOCKET_INVALID)
    {
        return;
    }

    packUserInterfaceDrawData(pDrawData, get_write_buffer(&pRemoteAppServer->mAwaitingSendDrawData));
    swap_buffers(&pRemoteAppServer->mAwaitingSendDrawData);
}

void remoteControlSendTexture(TinyImageFormat format, uint64_t textureId, uint32_t width, uint32_t height, uint32_t size,
                              unsigned char* ptr)
{
    if (!pRemoteAppServer || pRemoteAppServer->mServerConnectionSocket == SOCKET_INVALID)
    {
        return;
    }

    unsigned char*                    data = get_write_buffer(&pRemoteAppServer->mAwaitingSendTextureData);
    RemoteCommandUserInterfaceTexture textureCommand = {};
    textureCommand.mHeader = RemoteCommandHeader{ (uint32_t)sizeof(RemoteCommandUserInterfaceTexture) + size, REMOTE_MESSAGE_TEXTURE };
    textureCommand.mTextureId = textureId;
    textureCommand.mWidth = width;
    textureCommand.mHeight = height;
    textureCommand.mTextureSize = size;
    textureCommand.mFormat = format;

    memcpy(data, &textureCommand, sizeof(RemoteCommandUserInterfaceTexture));
    memcpy(data + sizeof(RemoteCommandUserInterfaceTexture), ptr, size);
    swap_buffers(&pRemoteAppServer->mAwaitingSendTextureData);
}

bool remoteAppIsConnected() { return pRemoteAppServer && pRemoteAppServer->mServerConnectionSocket != SOCKET_INVALID; }

bool remoteAppShouldSendFontTexture()
{
    if (pRemoteAppServer && pRemoteAppServer->mShouldSendFontTexture)
    {
        pRemoteAppServer->mShouldSendFontTexture = false;
        return true;
    }
    return false;
}

/****************************************************************************/
// MARK: - Remote Control Client
/****************************************************************************/

#ifdef FORGE_TOOLS // Only the UIRemoteControl tool can be the remote control client

/// Unpack the stream of bytes to UserInterfaceDrawData
static void unpackUserInterfaceDrawData(unsigned char* pData, UserInterfaceDrawData* drawData)
{
    RemoteCommandUserInterfaceDrawData* remoteDrawData = (RemoteCommandUserInterfaceDrawData*)pData;
    size_t                              offset = sizeof(RemoteCommandUserInterfaceDrawData);

    drawData->mDisplayPos = remoteDrawData->mDisplayPos;
    drawData->mDisplaySize = remoteDrawData->mDisplaySize;

    if (drawData->mNumDrawCommands < remoteDrawData->mNumDrawCommands)
    {
        if (drawData->mDrawCommands)
        {
            tf_free(drawData->mDrawCommands);
        }
        drawData->mDrawCommands = (UserInterfaceDrawCommand*)tf_calloc(remoteDrawData->mNumDrawCommands, sizeof(UserInterfaceDrawCommand));
    }
    drawData->mNumDrawCommands = remoteDrawData->mNumDrawCommands;

    for (uint32_t i = 0; i < drawData->mNumDrawCommands; i++)
    {
        RemoteCommandUserInterfaceDrawCommand* remoteDrawCommand = (RemoteCommandUserInterfaceDrawCommand*)(pData + offset);
        offset += sizeof(RemoteCommandUserInterfaceDrawCommand);

        drawData->mDrawCommands[i] = remoteDrawCommand->mElement;

        TextureNode* node = hmgetp(pRemoteControlClient->mTextureHashmap, remoteDrawCommand->mElement.mTextureId);

        if (node)
        {
            drawData->mDrawCommands[i].mTextureId = (uint64_t)node->value;
        }
        else
        {
            drawData->mDrawCommands[i].mTextureId = 1;
        }
    }

    if (drawData->mVertexCount * drawData->mVertexSize < remoteDrawData->mVertexCount * remoteDrawData->mVertexSize)
    {
        if (drawData->mVertexBufferData)
        {
            tf_free(drawData->mVertexBufferData);
        }
        drawData->mVertexBufferData = (unsigned char*)tf_malloc((size_t)remoteDrawData->mVertexCount * remoteDrawData->mVertexSize);
    }

    if (drawData->mIndexCount * drawData->mIndexSize < remoteDrawData->mIndexCount * remoteDrawData->mIndexSize)
    {
        if (drawData->mIndexBufferData)
        {
            tf_free(drawData->mIndexBufferData);
        }
        drawData->mIndexBufferData = (unsigned char*)tf_malloc((size_t)remoteDrawData->mIndexCount * remoteDrawData->mIndexSize);
    }

    size_t vertexSizeAligned = ((size_t)remoteDrawData->mVertexCount * remoteDrawData->mVertexSize / 8 + 1) * 8;
    size_t indexSizeAligned = ((size_t)remoteDrawData->mIndexCount * remoteDrawData->mIndexSize / 8 + 1) * 8;

    memcpy(drawData->mVertexBufferData, pData + offset, (size_t)remoteDrawData->mVertexCount * remoteDrawData->mVertexSize);
    offset += vertexSizeAligned;
    memcpy(drawData->mIndexBufferData, pData + offset, (size_t)remoteDrawData->mIndexCount * remoteDrawData->mIndexSize);
    offset += indexSizeAligned;

    drawData->mVertexCount = remoteDrawData->mVertexCount;
    drawData->mIndexCount = remoteDrawData->mIndexCount;
    drawData->mVertexSize = remoteDrawData->mVertexSize;
    drawData->mIndexSize = remoteDrawData->mIndexSize;
}

static void removeUserInterfaceDrawData(UserInterfaceDrawData* data)
{
    if (data)
    {
        tf_free(data->mVertexBufferData);
        tf_free(data->mIndexBufferData);
        tf_free(data->mDrawCommands);
        *data = {};
    }
}

/// Disconnect from a server app. Disconnects from all related sockets, and free memory allocated for the connection.
void remoteControlDisconnect()
{
    if (pRemoteControlClient)
    {
        pRemoteControlClient->mDisconnect = true;

        if (pRemoteControlClient->mClientThread)
        {
            joinThread(pRemoteControlClient->mClientThread);
        }
    }
}

static bool clientSend(Socket* socket)
{
    if (pRemoteControlClient->mDisconnect)
    {
        sendDisconnect(socket);
        // Ensure the server receives the disconnect and stop gracefully.
        threadSleep(100);
        return false;
    }

    bool sendSucceed = true;

    unsigned char* inputDataToSend = get_read_buffer(&pRemoteControlClient->mAwaitingSendInput);
    if (inputDataToSend)
    {
        sendSucceed =
            sendSucceed && (bufferedSend(socket, inputDataToSend, ((RemoteCommandUserInterfaceInput*)inputDataToSend)->mHeader.mSize) > 0);
        mark_buffer_processed(&pRemoteControlClient->mAwaitingSendInput);
    }

    if (!sendSucceed || *socket == SOCKET_INVALID)
    {
        return false;
    }

    sendSucceed = sendPing(socket);

    return sendSucceed;
}

static void clientReceive(Socket* socket)
{
    bool pingReceived = false;

    while (!pingReceived)
    {
        RemoteCommandHeader remoteCommandHeader = {};
        bool                receivedHeader = bufferedReceive(socket, (unsigned char*)&remoteCommandHeader, sizeof(RemoteCommandHeader));

        if (receivedHeader)
        {
            switch (remoteCommandHeader.mType)
            {
            case REMOTE_MESSAGE_DRAW:
            {
                unsigned char* receivedDrawFrameData = get_write_buffer(&pRemoteControlClient->mReceivedDrawData);
                bufferedReceive(socket, receivedDrawFrameData + sizeof(RemoteCommandHeader),
                                remoteCommandHeader.mSize - sizeof(RemoteCommandHeader));
                swap_buffers(&pRemoteControlClient->mReceivedDrawData);
            }
            break;
            case REMOTE_MESSAGE_TEXTURE:
            {
                unsigned char* receivedTextureData = get_write_buffer(&pRemoteControlClient->mReceivedTextureData);
                bufferedReceive(socket, receivedTextureData + sizeof(RemoteCommandHeader),
                                remoteCommandHeader.mSize - sizeof(RemoteCommandHeader));
                swap_buffers(&pRemoteControlClient->mReceivedTextureData);
            }
            break;
            case REMOTE_MESSAGE_PING:
            {
                pRemoteControlClient->mLastPingTime = time(0);
                pingReceived = true;
            }
            break;
            case REMOTE_MESSAGE_DISCONNECT:
            {
                pRemoteControlClient->mDisconnect = true;
                pingReceived = true;
            }
            default:
                break;
            }
        }
        else
        {
            break;
        }
    }
}

static void client(void* pData)
{
    UNREF_PARAM(pData);
    while (pRemoteControlClient->mClientConnectionSocket)
    {
        bool sendSucceed = clientSend(&pRemoteControlClient->mClientConnectionSocket);

        if (!sendSucceed)
        {
            break;
        }

        clientReceive(&pRemoteControlClient->mClientConnectionSocket);
        threadSleep(10);
    }

    LOGF(LogLevel::eINFO, "Disconnected from server.");
    if (pRemoteControlClient->mClientConnectionSocket != SOCKET_INVALID)
    {
        socketDestroy(&pRemoteControlClient->mClientConnectionSocket);
    }

    // Free all awaiting to send inputs
    remove_circular_buffer(&pRemoteControlClient->mAwaitingSendInput);
    arrfree(pRemoteControlClient->inputs);

    // Free all received draw data
    remove_circular_buffer(&pRemoteControlClient->mReceivedDrawData);
    removeUserInterfaceDrawData(&pRemoteControlClient->mLastReceivedDrawData);
    remove_circular_buffer(&pRemoteControlClient->mReceivedTextureData);

    for (uint32_t i = 0; i < hmlen(pRemoteControlClient->mTextureHashmap); i++)
    {
        removeResource(pRemoteControlClient->mTextureHashmap[i].value);
    }
    hmfree(pRemoteControlClient->mTextureHashmap);

    tf_free(pRemoteControlClient);
    pRemoteControlClient = NULL;
}

/// Connect to a server app.
void remoteControlConnect(const char* hostName, uint16_t port)
{
    if (pRemoteControlClient != NULL)
    {
        return;
    }

    pRemoteControlClient = (RemoteControlClient*)tf_malloc(sizeof(RemoteControlClient));
    *pRemoteControlClient = {};

    SocketAddr addr = {};
    socketAddrFromHostnamePort(&addr, hostName, port);
    socketCreateClient(&pRemoteControlClient->mClientConnectionSocket, &addr);

    if (pRemoteControlClient->mClientConnectionSocket != SOCKET_INVALID)
    {
        init_circular_buffer(&pRemoteControlClient->mAwaitingSendInput, InputBufferSize);
        init_circular_buffer(&pRemoteControlClient->mReceivedDrawData, DrawBufferSize);
        init_circular_buffer(&pRemoteControlClient->mReceivedTextureData, DrawBufferSize);

        ThreadDesc threadDesc = {};
        threadDesc.pFunc = client;
        threadDesc.pData = NULL;
        strncpy(threadDesc.mThreadName, "RemoteControlClientThread", sizeof(threadDesc.mThreadName));
        initThread(&threadDesc, &pRemoteControlClient->mClientThread);
    }
    else
    {
        tf_free(pRemoteControlClient);
        pRemoteControlClient = NULL;
    }
}

bool remoteControlIsConnected() { return pRemoteControlClient && pRemoteControlClient->mClientConnectionSocket != SOCKET_INVALID; }

/// Get the latest user interface draw data received from the server.
UserInterfaceDrawData* remoteControlReceiveDrawData()
{
    if (!pRemoteControlClient || pRemoteControlClient->mClientConnectionSocket == SOCKET_INVALID)
    {
        return NULL;
    }

    unsigned char* drawData = get_read_buffer(&pRemoteControlClient->mReceivedDrawData);
    if (drawData)
    {
        unpackUserInterfaceDrawData(drawData, &pRemoteControlClient->mLastReceivedDrawData);
        mark_buffer_processed(&pRemoteControlClient->mReceivedDrawData);
    }

    return &pRemoteControlClient->mLastReceivedDrawData;
}

void remoteControlReceiveTexture()
{
    if (!pRemoteControlClient || pRemoteControlClient->mClientConnectionSocket == SOCKET_INVALID)
    {
        return;
    }

    RemoteCommandUserInterfaceTexture* textureData =
        (RemoteCommandUserInterfaceTexture*)get_read_buffer(&pRemoteControlClient->mReceivedTextureData);
    if (textureData)
    {
        SyncToken       token = {};
        Texture*        texture;
        TextureLoadDesc loadDesc = {};
        TextureDesc     desc = {};

        desc.mWidth = textureData->mWidth;
        desc.mHeight = textureData->mHeight;
        desc.mDepth = 1;
        desc.mArraySize = 1;
        desc.mMipLevels = 1;
        desc.mFormat = (TinyImageFormat)textureData->mFormat;
        desc.mStartState = RESOURCE_STATE_SHADER_RESOURCE;
        desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
        desc.mSampleCount = SAMPLE_COUNT_1;
        loadDesc.pDesc = &desc;
        loadDesc.ppTexture = &texture;

        addResource(&loadDesc, &token);
        waitForToken(&token);

        TextureUpdateDesc updateDesc = { texture };
        updateDesc.mCurrentState = RESOURCE_STATE_SHADER_RESOURCE;
        beginUpdateResource(&updateDesc);
        TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
        memcpy(subresource.pMappedData, ((unsigned char*)textureData) + sizeof(RemoteCommandUserInterfaceTexture),
               textureData->mTextureSize);
        endUpdateResource(&updateDesc);

        hmput(pRemoteControlClient->mTextureHashmap, textureData->mTextureId, texture);

        mark_buffer_processed(&pRemoteControlClient->mReceivedTextureData);
    }
}

/// Process the input and store it as an awaiting to send input.
void remoteControlCollectInputData(uint32_t actionId, bool buttonPress, const float2* mousePos, const float2 stick)
{
    if (!pRemoteControlClient || pRemoteControlClient->mClientConnectionSocket == SOCKET_INVALID)
    {
        return;
    }

    RemoteCommandUserInterfaceInputData input = {};
    input.mActionId = actionId;
    input.mButtonPress = buttonPress;
    input.mStick = stick;
    input.mSkipMouse = mousePos == NULL;
    if (mousePos)
    {
        input.mMousePos = *mousePos;
    }

    arrpush(pRemoteControlClient->inputs, input);
}

/// Send all the awaiting inputs.
void remoteControlSendInputData()
{
    if (!pRemoteControlClient || pRemoteControlClient->mClientConnectionSocket == SOCKET_INVALID)
    {
        return;
    }

    uint32_t numInputs = (uint32_t)arrlen(pRemoteControlClient->inputs);

    if (numInputs == 0)
    {
        return;
    }

    // Check if we have not sent the previous input data yet.
    // If we have not sent it yet, combine it with the new input data.

    unsigned char* packedData = get_write_buffer(&pRemoteControlClient->mAwaitingSendInput);
    uint32_t       numPrevInputs = 0;
    numPrevInputs = ((RemoteCommandUserInterfaceInput*)packedData)->mNumInputs;
    size_t offset = sizeof(RemoteCommandUserInterfaceInput) + sizeof(RemoteCommandUserInterfaceInputData) * numPrevInputs;

    size_t packetSize = sizeof(RemoteCommandUserInterfaceInput);
    packetSize += (numPrevInputs + numInputs) * sizeof(RemoteCommandUserInterfaceInputData);

    RemoteCommandUserInterfaceInput drawData = {};
    drawData.mHeader.mSize = (uint32_t)packetSize;
    drawData.mNumInputs = numInputs + numPrevInputs;
    memcpy(packedData, &drawData, sizeof(RemoteCommandUserInterfaceInput));

    for (uint32_t i = 0; i < numInputs; i++)
    {
        memcpy(packedData + offset, &pRemoteControlClient->inputs[i], sizeof(RemoteCommandUserInterfaceInputData));
        offset += sizeof(RemoteCommandUserInterfaceInputData);
    }

    arrsetlen(pRemoteControlClient->inputs, 0);
    swap_buffers(&pRemoteControlClient->mAwaitingSendInput);
}
#endif // FORGE_TOOLS

#endif