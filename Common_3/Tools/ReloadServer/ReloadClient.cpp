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

/*
How ShaderServer works:
    1. When shaders for App are first compiled, the args/directories used for `fsl.py` are cached to disk
    2. Shader recompilation is triggered by pressing `Reload Shaders` button or CTRL+S (and shader-reloading is temporarily disabled)
    3. App connects to ShaderServer daemon on host PC and sends directories to be used for recompilation
    4. ShaderServer daemon loads the cached `fsl.py` args from the directories sent by App
    5. ShaderServer daemon recompiles shaders using cached args and uploads modified shaders back to App
    6. App caches uploaded shaders and sets `gClient.mDidReload`
    7. On next call to `App.update`, `platformReloadClientShouldQuit` calls `requestReload(ReloadDesc{ RELOAD_TYPE_SHADER })` (and
shader-reloading is enabled again)
    8. When shaders are reloaded, `platformReloadClientGetShaderBinary` checks if the given path was updated and provides the new shader
bytecode
    9. Shader changes are now visible in App

If an error occurs on the device:
    1. Error is printed using LOGF(eError) and recompile process is stopped
    2. Shader-reloading is re-enabled

If an error occurs on the host:
    1. ShaderServer daemon sends error message to device via socket
    2. Error is printed using LOGF(eError) and recompile process is stopped
    3. Shader-reloading is re-enabled
*/

#include "ReloadClient.h"

#include <string.h>

#include "../../Application/Interfaces/IInput.h"
#include "../../Application/Interfaces/IUI.h"
#include "../../Game/Interfaces/IScripting.h"
#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"

#include "../../Utilities/Threading/Atomics.h"
#include "../Network/Network.h"

#ifdef _MSC_VER
#define tfrg_byteswap32(x) _byteswap_ulong(x)
#else
#define tfrg_byteswap32(x) __builtin_bswap32(x)
#endif

#if defined(__APPLE__) && !defined(TARGET_IOS) && !defined(TARGET_IOS_SIMULATOR)
#define TARGET_MACOS
#endif

#if (defined(_WINDOWS) && !defined(XBOX)) || defined(TARGET_MACOS) || (defined(__linux__) && !defined(ANDROID))
#define TARGET_PC
#endif

// You can explicitly disable auto-start by defining `SHADER_SERVER_DISABLE_AUTO_START`
#if defined(TARGET_PC) && !defined(SHADER_SERVER_DISABLE_AUTO_START)
#define SHADER_SERVER_AUTO_START_ENABLED
#endif

// NOTE: There is no good cross-platform method of checking endianness at compile time
static bool isLittleEndian()
{
    int i = 1;
    // to detect, we cast `i` to bytes and read the first one
    // hex			      00 00 00 01
    // if little endian: [ 1, 0, 0, 0]	-> true
    // if big endian   : [ 0, 0, 0, 1]	-> false
    return (int)*((unsigned char*)&i) == 1;
}

#define MSG_SHADER         's'
#define MSG_RECOMPILE      'r'
#define MSG_SUCCESS        '\x00'
#define MSG_ERROR          '\x01'

#define SHADER_SERVER_FILE "shader-server.txt"

#if defined(LINUX) || defined(TARGET_MACOS) || defined(TARGET_IOS) || defined(TARGET_IOS_SIMULATOR)
#define DAEMON_CMD "Common_3/Tools/ReloadServer/ReloadServer.sh"
#else
#define DAEMON_CMD "Common_3\\Tools\\ReloadServer\\ReloadServer.bat"
#endif

typedef struct ReloadClient
{
    SocketAddr           mAddr;
    Mutex                mLock;
    ThreadHandle         mThread;
    uint8_t*             pUpdatedShaders;
    UIComponent*         pReloadShaderComponent;
    tfrg_atomic32_t      mDidReload;
    tfrg_atomic32_t      mShouldReenableButton;
    tfrg_atomic32_t      mIsReloading;
    uint16_t             mPort;
    bool                 mDidInit;
    bstring              mHost;
    bstring              mProjectDir;
    bstring              mBinaryDir;
    bstring              mIntermediateDir;
    // File contains line for each of (host, port, projectDir, binaryDir, intermediateDir)
    // Each line can have a max length of FS_MAX_PATH + 2, where 2 is the max number of line terminators '\r\n'.
    // Therefore max size of the entire file is 5 * (FS_MAX_PATH + 2).
    static constexpr int ExpectedLineCount = 5;
    char                 mShaderServerFileBuf[ExpectedLineCount * (FS_MAX_PATH + 2)];
} ReloadClient;

static ReloadClient gClient{};

#if defined(AUTOMATED_TESTING)
uint32_t gShaderServerRequestRecompileAfter = 0;
#endif

static uint32_t decodeU32LE(uint8_t** pBuffer)
{
    uint32_t value;
    // NOTE: We can just cast and dereference here but I think the memcpy is clearer,
    // and it should produce identical assembly.
    memcpy(&value, *pBuffer, sizeof(value));
    *pBuffer += sizeof(uint32_t);
    return isLittleEndian() ? value : tfrg_byteswap32(value);
}

static void encodeU32LE(uint8_t** pBuffer, uint32_t value)
{
    uint32_t toEncode = isLittleEndian() ? value : tfrg_byteswap32(value);
    // NOTE: We can just cast and dereference here but I the memcpy is clearer,
    // and it should produce identical assembly.
    memcpy(*pBuffer, &toEncode, sizeof(value));
    *pBuffer += sizeof(uint32_t);
}

static bool readShaderServerFile()
{
    FileStream stream;
    if (!fsOpenStreamFromPath(RD_SHADER_BINARIES, SHADER_SERVER_FILE, FM_READ, &stream))
    {
        LOGF(eERROR,
             "Failed to read `%s` from disk in mount point RD_SHADER_BINARIES. "
             "This is likely an internal bug - please check that this file is in the `CompiledShaders` directory.",
             SHADER_SERVER_FILE);
        return false;
    }

    size_t nRead = fsReadFromStream(&stream, gClient.mShaderServerFileBuf, sizeof(gClient.mShaderServerFileBuf));
    if (nRead > sizeof(gClient.mShaderServerFileBuf))
    {
        LOGF(eERROR,
             "Read too many bytes from `%s` - this likely means the file was not written correctly."
             "Please check that the contents of this file have not been corrupted.",
             SHADER_SERVER_FILE);
        return false;
    }
    fsCloseStream(&stream);

    bstring contents = bconstfromblk(gClient.mShaderServerFileBuf, (int)min(nRead, sizeof(gClient.mShaderServerFileBuf)));

    struct SplitData
    {
        bstring*    contents;
        uint32_t    lineCount;
        const char* lines[ReloadClient::ExpectedLineCount];
    };
    SplitData splitData = { &contents, 0, {} };
    int       ret = bsplitcb(
        &contents, '\n', 0,
        [](void* param, int ofs, int len)
        {
            if (len)
            {
                SplitData* data = (SplitData*)param;
                if (data->lineCount >= ReloadClient::ExpectedLineCount)
                {
                    return -1;
                }
                char* line = bdataofs(data->contents, ofs);
                ASSERT(line);
                // Replace `\n` with NULL so that each line is a NULL-terminated string (needed for `atoi`, also makes constructing bstrings
                // easier). Also, if the string ends in `\r\n`, we want to replace `\r` with 0 instead of `\n`.
                line[len - (line[len - 1] == '\r')] = 0;
                data->lines[data->lineCount++] = line;
            }
            return 0;
        },
        &splitData);

    if (ret == -1 || splitData.lineCount != ReloadClient::ExpectedLineCount)
    {
        LOGF(eERROR,
             "`%s` is not in the correct format - this likely means the file was not written correctly."
             "Please check that the contents of this file have not been corrupted.",
             SHADER_SERVER_FILE);
        return false;
    }

    // On PC we just use localhost since the host PC and device are the same machine.
    // On Android we can use `adb reverse tcp:PORT tcp:PORT` to forward requests to localhost:PORT
    // on the device back to localhost:PORT on the host PC, which avoids requiring a network connection.
#if defined(_WINDOWS) || defined(TARGET_MACOS) || defined(LINUX) || defined(ANDROID)
    gClient.mHost = bconstfromliteral("127.0.0.1");
#else
    gClient.mHost = bconstfromcstr(splitData.lines[0]);
#endif
    gClient.mPort = atoi(splitData.lines[1]);
    gClient.mProjectDir = bconstfromcstr(splitData.lines[2]);
    gClient.mBinaryDir = bconstfromcstr(splitData.lines[3]);
    gClient.mIntermediateDir = bconstfromcstr(splitData.lines[4]);

    const char* host = bdata(&gClient.mHost);
    ASSERT(host);

    if (!socketAddrFromHostPort(&gClient.mAddr, host, gClient.mPort))
    {
        LOGF(eERROR, "Failed to create socket address from invalid address `%s:%u`", host, gClient.mPort);
        return false;
    }

    return true;
}

extern const char* getShaderPlatformName();

static bool sendRecompileRequestToServer(Socket* pSock)
{
    uint8_t  buffer[1 + 3 * (FS_MAX_PATH + 5)];
    uint8_t* ptr = buffer;
    *(ptr++) = MSG_RECOMPILE;
    const char* toSend[] = { getShaderPlatformName(), bdata(&gClient.mProjectDir), bdata(&gClient.mBinaryDir),
                             bdata(&gClient.mIntermediateDir) };
    uint32_t    toSendLen[] = { (uint32_t)strnlen(toSend[0], FS_MAX_PATH), (uint32_t)blength(&gClient.mProjectDir),
                                (uint32_t)blength(&gClient.mBinaryDir), (uint32_t)blength(&gClient.mIntermediateDir) };
    for (int i = 0; i < 4; ++i)
    {
        encodeU32LE(&ptr, toSendLen[i]);
        if (toSend[i])
        {
            memcpy(ptr, toSend[i], toSendLen[i]);
        }
        ptr += toSendLen[i];
        *(ptr++) = 0;
    }

    size_t numBytesSent = 0;
    size_t numBytesToSend = (size_t)(ptr - buffer);
    while (numBytesSent < numBytesToSend)
    {
        ssize_t nBytes = socketSend(pSock, buffer + numBytesSent, numBytesToSend - numBytesSent);
        if (nBytes <= 0)
        {
            return false;
        }
        numBytesSent += (size_t)nBytes;
    }

    return true;
}

// NOTE: Functions that allocate memory are undesirable, but this is a private
// implementation detail used to make `requestRecompileThreadFunc` a bit cleaner.
static uint8_t* waitForServerToUploadShaders(Socket* pSock, bool* serverDidReturnError)
{
    char msgAndSize[5];
    if (socketRecv(pSock, &msgAndSize, 5) < 5)
    {
        LOGF(eERROR, "Failed to read upload header (5 bytes), aborting shader upload...");
        return NULL;
    }
    if (msgAndSize[0] != MSG_SHADER && msgAndSize[0] != MSG_ERROR)
    {
        LOGF(eERROR, "Invalid upload header tag byte `%c` - expected `%c`, aborting shader upload...", msgAndSize[0], MSG_SHADER);
        return NULL;
    }
    *serverDidReturnError = msgAndSize[0] == MSG_ERROR;

    uint8_t* ptrSize = (uint8_t*)&msgAndSize[1];
    uint32_t msgSize = decodeU32LE(&ptrSize);

    uint8_t* buffer = (uint8_t*)tf_malloc(msgSize);
    memset(buffer, 0, msgSize);

    size_t total = 0;
    size_t remaining = msgSize;
    while (remaining)
    {
        ssize_t received = socketRecv(pSock, buffer + total, remaining);
        if (received < 0)
        {
            LOGF(eERROR, "`socketRecv` failed during upload of shaders, aborting shader upload...");
            tf_free(buffer);
            return NULL;
        }
        total += (size_t)received;
        remaining -= (size_t)received;
        if (received == 0)
        {
            if (remaining)
            {
                tf_free(buffer);
                return NULL;
            }
            else
            {
                break;
            }
        }
    }

    return buffer;
}

static void requestRecompileThreadFunc(void* userdata)
{
    MutexLock lock(gClient.mLock);

    const char* host = bdata(&gClient.mHost);
    ASSERT(host);

    Socket sock;
    int    tryCount = 0;
    bool   success = false;
    while (!success && tryCount < 5)
    {
        LOGF(eDEBUG, "Connecting to ShaderServer...");
        success = socketCreateClient(&sock, &gClient.mAddr);
        if (!success)
        {
            threadSleep(1000);
        }
        ++tryCount;
    }

    if (!success)
    {
        LOGF(eERROR,
             "Failed to connect to `ShaderServer` on address `%s:%u`. "
             "Please ensure `ShaderServer` is running by executing `%s` in a terminal.",
             host, (uint32_t)gClient.mPort, DAEMON_CMD);
        tfrg_atomic32_store_release(&gClient.mShouldReenableButton, 1);
        return;
    }

    LOGF(eDEBUG, "Sending recompile request to ShaderServer");
    if (!sendRecompileRequestToServer(&sock))
    {
        LOGF(eERROR,
             "Failed to send recompile request to `ShaderServer` on address `%s:%u`. "
             "Please ensure `ShaderServer` is running by executing `%s` in a terminal.",
             host, (uint32_t)gClient.mPort, DAEMON_CMD);
        socketDestroy(&sock);
        tfrg_atomic32_store_release(&gClient.mShouldReenableButton, 1);
        return;
    }

    LOGF(eDEBUG, "Waiting for response from ShaderServer");
    bool     serverDidReturnError = false;
    uint8_t* data = waitForServerToUploadShaders(&sock, &serverDidReturnError);
    if (!data)
    {
        LOGF(eERROR,
             "Shader upload from `ShaderServer` on address `%s:%u` was interrupted. "
             "Please ensure `ShaderServer` is running by executing `%s` in a terminal.",
             host, (uint32_t)gClient.mPort, DAEMON_CMD);
        socketDestroy(&sock);
        tfrg_atomic32_store_release(&gClient.mShouldReenableButton, 1);
        return;
    }

    if (serverDidReturnError)
    {
        LOGF(eERROR, "%s", (const char*)data);
        tf_free(data);
    }
    else
    {
        LOGF(eDEBUG, "ShaderServer recompile success!");
        if (gClient.pUpdatedShaders)
        {
            tf_free(gClient.pUpdatedShaders);
        }
        gClient.pUpdatedShaders = data;
        tfrg_atomic32_store_release(&gClient.mDidReload, 1);
    }
    tfrg_atomic32_store_release(&gClient.mShouldReenableButton, 1);

    if (!socketDestroy(&sock))
    {
        LOGF(eERROR,
             "Failed to destroy socket %zu. This does not affect shader recompilation at all, "
             "but does likely indicate an internal bug that should be investigated.",
             (size_t)sock);
    }
}

#ifdef SHADER_SERVER_AUTO_START_ENABLED

#include "../../Utilities/Interfaces/IToolFileSystem.h"

// NOTE: This can also be done with IDE properties/build-time macros, but that ends up being
// quite ugly, difficult to understand, and far away from this code that needs it.
// If ReloadClient.cpp is ever moved, this needs to be updated.
#define THE_FORGE_ROOT_DIR        __FILE__ "/../../../.."

#define THE_FORGE_PYTHON_PATH     "Tools/python-3.6.0-embed-amd64/python.exe"
#define SHADER_SERVER_SCRIPT_PATH "Common_3/Tools/ReloadServer/ReloadServer.py"

typedef enum ShaderServerAction
{
    SHADER_SERVER_ACTION_START,
    SHADER_SERVER_ACTION_KILL,
} ShaderServerAction;

void platformStartStopShaderServerOnHost(ShaderServerAction action)
{
    const bool kill = action == SHADER_SERVER_ACTION_KILL;

    char rootTheForge[FS_MAX_PATH] = {};
    fsNormalizePath(THE_FORGE_ROOT_DIR, '/', rootTheForge);

#ifdef _WINDOWS
    char python[FS_MAX_PATH] = {};
    fsAppendPathComponent(rootTheForge, THE_FORGE_PYTHON_PATH, python);
#else
    const char* python = "python3";
#endif

    char scriptPath[FS_MAX_PATH] = {};
    fsAppendPathComponent(rootTheForge, SHADER_SERVER_SCRIPT_PATH, scriptPath);

    char port[64] = {};
    snprintf(port, sizeof(port), "%u", gClient.mPort);

    const char* args[] = { scriptPath, kill ? "--kill" : "--daemon", "--port", port };

    const char* outputFilename = kill ? "ShaderServerKill.txt" : "ShaderServerStart.txt";

    char outputPath[FS_MAX_PATH] = {};
    fsAppendPathComponent(fsGetResourceDirectory(RD_DEBUG), outputFilename, outputPath);

    int ret = systemRun(python, args, sizeof(args) / sizeof(args[0]), outputPath);
    if (ret != 0)
    {
        FileStream fs;
        if (fsOpenStreamFromPath(RD_DEBUG, outputFilename, FM_READ, &fs))
        {
            ssize_t size = fsGetStreamFileSize(&fs);
            char*   output = (char*)tf_malloc(size + 1);
            size_t  nRead = fsReadFromStream(&fs, (void*)output, (size_t)size);
            fsCloseStream(&fs);
            ASSERT(nRead == (size_t)size);
            output[size] = '\0';
            const char* action = kill ? "killing" : "starting";
            LOGF(eERROR, "Error %s the ShaderServer process:\n%s\n", action, output);
            tf_free(output);
        }
    }
}

#endif

bool platformInitReloadClient(void)
{
    if (!initNetwork())
    {
        LOGF(eERROR, "Failed to initialize network library.");
        return false;
    }

    if (!initMutex(&gClient.mLock))
    {
        LOGF(eERROR, "Failed to initialize thread mutex.");
        return false;
    }

    if (!readShaderServerFile())
    {
        // error messages are printed by `readShaderServerFile`
        return false;
    }

    SocketAddr  addr;
    const char* host = bdata(&gClient.mHost);
    ASSERT(host);
    if (!socketAddrFromHostPort(&addr, host, gClient.mPort))
    {
        LOGF(eERROR, "Failed to create network address from host `%s` and port `%u`.", host, (uint32_t)gClient.mPort);
        return false;
    }
    gClient.mDidInit = true;
    tfrg_atomic32_store_release(&gClient.mDidReload, 0);
    tfrg_atomic32_store_release(&gClient.mShouldReenableButton, 0);
    tfrg_atomic32_store_release(&gClient.mIsReloading, 0);

#ifdef SHADER_SERVER_AUTO_START_ENABLED
    platformStartStopShaderServerOnHost(SHADER_SERVER_ACTION_START);
#endif

    return true;
}

void platformExitReloadClient()
{
    if (!gClient.mDidInit)
    {
        return;
    }

    acquireMutex(&gClient.mLock);
    releaseMutex(&gClient.mLock);

#ifdef SHADER_SERVER_AUTO_START_ENABLED
    platformStartStopShaderServerOnHost(SHADER_SERVER_ACTION_KILL);
#endif

    destroyMutex(&gClient.mLock);

    if (gClient.pUpdatedShaders)
    {
        tf_free(gClient.pUpdatedShaders);
    }

    exitNetwork();

    memset(&gClient, 0, sizeof(ReloadClient));
}

void platformReloadClientRequestShaderRecompile()
{
    if (tfrg_atomic32_cas_relaxed(&gClient.mIsReloading, 0, 1) == 1)
    {
        // Someone has already requested a recompile so we don't need to
        return;
    }

    uiSetComponentActive(gClient.pReloadShaderComponent, false);

    ThreadDesc desc = { requestRecompileThreadFunc, nullptr, "ShaderRecompile" };
    if (!initThread(&desc, &gClient.mThread))
    {
        LOGF(eERROR, "Failed to start ShaderRequestRecompile thread - this is likely an internal bug.");
        ASSERT(false);
    }
}

bool platformReloadClientGetShaderBinary(const char* path, void** pByteCode, uint32_t* pByteCodeSize)
{
    if (!gClient.mDidInit)
    {
        return false;
    }

    MutexLock lock(gClient.mLock);

    uint8_t* ptr = gClient.pUpdatedShaders;
    if (!ptr)
    {
        return false;
    }

    // NOTE: Even for a large number of shaders the loop below is quite fast, so we just search
    // through all the shaders instead of having to worry about additional memory allocations
    // to store the parsed data/hashmaps/etc.
    uint32_t nShaders = decodeU32LE(&ptr);
    for (uint32_t i = 0; i < nShaders; ++i)
    {
        uint32_t    fileNameSize = decodeU32LE(&ptr);
        uint32_t    byteCodeSize = decodeU32LE(&ptr);
        const char* fileName = (const char*)ptr;
        ptr += fileNameSize;
        ptr += 1;
        uint8_t* byteCode = ptr;
        ptr += byteCodeSize;

        if (strncmp(fileName, path, FS_MAX_PATH) == 0)
        {
            LOGF(eDEBUG, "Updated shader %s - %u bytes", fileName, byteCodeSize);
            *pByteCode = (void*)byteCode;
            *pByteCodeSize = byteCodeSize;
            return true;
        }
    }

    return false;
}

// NOTE: Since `requestReload` is not thread safe, this function allows us to
// call `requestReload` on the main thread when shaders have been uploaded,
// and exit the application when testing (by returning true).
bool platformReloadClientShouldQuit(void)
{
#if defined(AUTOMATED_TESTING)
    static uint32_t nFrames = 0;
    if (gShaderServerRequestRecompileAfter != 0 && ++nFrames == gShaderServerRequestRecompileAfter)
    {
        platformReloadClientRequestShaderRecompile();
    }
#endif

    // TODO: Should we re-enable `Reload shaders` button after X time has passed, even if the thread might still be running?
    // This could be problematic but might be a good fallback for the recompile thread dying for strange reasons.
    if (tfrg_atomic32_store_release(&gClient.mShouldReenableButton, 0) == 1)
    {
        // We need to join here, but the thread should be done though since setting mShouldReenableButton is one of the last things it does.
        ASSERT(gClient.mThread);
        joinThread(gClient.mThread);
        gClient.mThread = INVALID_THREAD_ID;
        tfrg_atomic32_store_release(&gClient.mIsReloading, 0);
        uiSetComponentActive(gClient.pReloadShaderComponent, true);
    }

    if (tfrg_atomic32_store_release(&gClient.mDidReload, 0) == 1)
    {
#if defined(AUTOMATED_TESTING)
        return true;
#else
        ReloadDesc desc{ RELOAD_TYPE_SHADER };
        requestReload(&desc);
#endif
    }
    return false;
}

void platformReloadClientAddReloadShadersButton(UIComponent* pReloadShaderComponent)
{
    if (!gClient.mDidInit)
    {
        return;
    }

    ButtonWidget shaderReload;
    UIWidget*    pShaderReload = uiCreateComponentWidget(pReloadShaderComponent, "Reload shaders", &shaderReload, WIDGET_TYPE_BUTTON);
    uiSetWidgetOnEditedCallback(pShaderReload, nullptr, [](void* pUserData) { platformReloadClientRequestShaderRecompile(); });
    REGISTER_LUA_WIDGET(pShaderReload);

    gClient.pReloadShaderComponent = pReloadShaderComponent;

    InputActionDesc actionDesc = { DefaultInputActions::RELOAD_SHADERS, [](InputActionContext* ctx)
                                   {
                                       platformReloadClientRequestShaderRecompile();
                                       return true;
                                   } };
    addInputAction(&actionDesc);
}
