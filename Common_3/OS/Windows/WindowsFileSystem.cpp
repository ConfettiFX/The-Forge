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

#include "../../Application/Config.h"

#if !defined(XBOX)
// clang-format off
#include "shlobj.h"
#include "commdlg.h"
#include <WinBase.h>
// clang-format on
#endif
#include <io.h>
#include <stdio.h>

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../Interfaces/IOperatingSystem.h"

#include "../../Utilities/Interfaces/IMemory.h"

template<typename T>
static inline T withUTF16Path(const char* path, T (*function)(const wchar_t*))
{
    size_t  len = strlen(path) + 1;
    wchar_t buffer[FS_MAX_PATH];
    ASSERT(len < FS_MAX_PATH);

    MultiByteToWideChar(CP_UTF8, 0, path, (int)len, buffer, (int)len);

    return function(buffer);
}

extern "C"
{
    extern ResourceDirectoryInfo gResourceDirectories[RD_COUNT];
    void                         parse_path_statement(char* PathStatment, size_t size);
}

#ifndef XBOX
static bool gInitialized = false;

bool initFileSystem(FileSystemInitDesc* pDesc)
{
    if (gInitialized)
    {
        LOGF(LogLevel::eWARNING, "FileSystem already initialized.");
        return true;
    }
    ASSERT(pDesc);

    if (!pDesc->mIsTool)
    {
        FILE* pathStatement = fopen(PATHSTATEMENT_FILE_NAME, "r");
        if (!pathStatement)
        {
            ASSERT(false);
            return false;
        }

        fseek(pathStatement, 0, SEEK_END);
        size_t fileSize = ftell(pathStatement);
        rewind(pathStatement);

        char* buffer = (char*)tf_malloc(fileSize);
        // size may change after converting \r\n into \n
        fileSize = fread(buffer, 1, fileSize, pathStatement);
        fclose(pathStatement);

        parse_path_statement(buffer, fileSize);
        tf_free(buffer);
    }

    gInitialized = true;
    return true;
}

void exitFileSystem(void) { gInitialized = false; }
#endif

struct WindowsFileStream
{
    FILE*  file;
    HANDLE handle;
    HANDLE fileMapping;
    LPVOID mapView;
};

#define WSD(name, fs) struct WindowsFileStream* name = (struct WindowsFileStream*)(fs)->mUser.data

class WindowsErrorString
{
    LPVOID lpMsgBuf;
    char   utf8[FS_MAX_PATH];

public:
    WindowsErrorString()
    {
        DWORD error = GetLastError();

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

        if (!WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)lpMsgBuf, -1, utf8, sizeof utf8, NULL, NULL))
        {
            snprintf(utf8, sizeof utf8, "0x%x", error);
        }
    }

    ~WindowsErrorString()
    {
        if (lpMsgBuf)
            LocalFree(lpMsgBuf);
    }

    const char* c_str() { return utf8; }
};

static inline FORGE_CONSTEXPR const char* fsFileModeToString(FileMode mode)
{
    mode = (FileMode)(mode & ~FM_ALLOW_READ);
    switch (mode)
    {
    case FM_READ:
        return "rb";
    case FM_WRITE:
        return "wb";
    case FM_WRITE_APPEND:
        return "ab";
    case FM_READ_WRITE:
        return "rb+";
    case FM_READ_APPEND:
        return "ab+";
    default:
        return "r";
    }
}

static inline FORGE_CONSTEXPR const char* fsOverwriteFileModeToString(FileMode mode)
{
    switch (mode)
    {
    case FM_READ_WRITE:
        return "wb+";
    default:
        return fsFileModeToString(mode);
    }
}

bool PlatformOpenFile(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
    memset(pOut, 0, sizeof *pOut);

    char filePath[FS_MAX_PATH] = { 0 };
    strcat_s(filePath, FS_MAX_PATH, gResourceDirectories[resourceDir].mPath);
    strcat_s(filePath, FS_MAX_PATH, fileName);

    // Path utf-16 conversion
    size_t  filePathLen = strlen(filePath) + 1;
    wchar_t pathStr[FS_MAX_PATH];
    ASSERT(filePathLen < FS_MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, filePath, (int)filePathLen, pathStr, (int)filePathLen);

    // Mode string utf-16 conversion
    const char* modeStr = fsFileModeToString(mode);

    wchar_t modeWStr[4] = {};
    mbstowcs(modeWStr, modeStr, 4);

    FILE* fp = NULL;
    if (mode & FM_ALLOW_READ)
    {
        fp = _wfsopen(pathStr, modeWStr, _SH_DENYWR);
    }
    else
    {
        _wfopen_s(&fp, pathStr, modeWStr);
    }

    // We need to change mode for read | write mode to 'w+' or 'wb+'
    // if file doesn't exist so that it can be created
    if (!fp)
    {
        // Try changing mode to 'w+' or 'wb+'
        if ((mode & FM_READ_WRITE) == FM_READ_WRITE)
        {
            modeStr = fsOverwriteFileModeToString(mode);
            mbstowcs(modeWStr, modeStr, 4);
            if (mode & FM_ALLOW_READ)
            {
                fp = _wfsopen(pathStr, modeWStr, _SH_DENYWR);
            }
            else
            {
                _wfopen_s(&fp, pathStr, modeWStr);
            }
        }
    }

    if (fp)
    {
        *pOut = {};
        pOut->mMode = mode;
        pOut->pIO = pSystemFileIO;

        WSD(stream, pOut);
        stream->file = fp;

        stream->handle = (HANDLE)_get_osfhandle(_fileno(fp));
        if (stream->handle == INVALID_HANDLE_VALUE)
        {
            LOGF(LogLevel::eERROR, "Error getting file handle for %s -- %s (error: %s)", filePath, modeStr, strerror(errno));
            fclose(fp);
            return false;
        }

        return true;
    }
    else
    {
        LOGF(LogLevel::eERROR, "Error opening file: %s -- %s (error: %s)", filePath, modeStr, strerror(errno));
    }

    return false;
}

static bool ioWindowsFsOpen(IFileSystem*, ResourceDirectory rd, const char* fileName, FileMode mode, FileStream* pOut)
{
    return PlatformOpenFile(rd, fileName, mode, pOut);
}

static ssize_t ioWindowsFsGetSize(FileStream* fs)
{
    WSD(stream, fs);
    LARGE_INTEGER fileSize = {};
    if (GetFileSizeEx(stream->handle, &fileSize))
        return (ssize_t)fileSize.QuadPart;
    LOGF(eERROR, "Failed to get file size: %s", WindowsErrorString().c_str());
    return -1;
}

static bool ioWindowsFsMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
{
    *outSize = 0;
    *outData = NULL;

    if (fs->mMode & FM_WRITE)
        return false;

    WSD(stream, fs);

    // mapped already
    if (!stream->mapView)
    {
        HANDLE fileMapping = CreateFileMappingW(stream->handle, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!fileMapping)
        {
            LOGF(eERROR, "Failed to CreateFileMappingW, %s", WindowsErrorString().c_str());
            return false;
        }

        ASSERT(GetLastError() != ERROR_ALREADY_EXISTS);

        LPVOID mem = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
        if (!mem)
        {
            LOGF(eERROR, "Failed to MapViewOfFile, %s", WindowsErrorString().c_str());
            CloseHandle(fileMapping);
            return false;
        }

        stream->fileMapping = fileMapping;
        stream->mapView = mem;
    }

    *outSize = ioWindowsFsGetSize(fs);
    *outData = stream->mapView;
    return true;
}

static bool ioWindowsFsClose(FileStream* fs)
{
    WSD(stream, fs);

    if (stream->mapView && !UnmapViewOfFile(stream->mapView))
    {
        LOGF(eERROR, "Error unmapping file: %s", WindowsErrorString().c_str());
    }

    if (stream->fileMapping != INVALID_HANDLE_VALUE)
    {
        CloseHandle(stream->fileMapping);
        stream->fileMapping = INVALID_HANDLE_VALUE;
    }

    if (fclose(stream->file) == EOF)
    {
        LOGF(LogLevel::eERROR, "Error closing system FileStream: %s (%x)", strerror(errno), errno);
        return false;
    }

    return true;
}

static size_t ioWindowsFsRead(FileStream* fs, void* dst, size_t size)
{
    WSD(stream, fs);
    size_t read = fread(dst, 1, size, stream->file);
    if (read != size && ferror(stream->file))
    {
        LOGF(LogLevel::eERROR, "Error reading %s bytes from file: %s (%x)", humanReadableSize(size).str, strerror(errno), errno);
    }
    return read;
}

static size_t ioWindowsFsWrite(FileStream* fs, const void* src, size_t size)
{
    if ((fs->mMode & (FM_WRITE | FM_APPEND)) == 0)
    {
        LOGF(LogLevel::eERROR, "Writing to FileStream with mode %i", fs->mMode);
        return 0;
    }

    WSD(stream, fs);
    size_t written = fwrite(src, 1, size, stream->file);

    if (written != size)
    {
        LOGF(LogLevel::eERROR, "Error writing to system FileStream: %s (%x)", strerror(errno), errno);
    }

    return written;
}

static bool ioWindowsFsSeek(FileStream* fs, SeekBaseOffset base, ssize_t offset)
{
    int origin = SEEK_SET;
    switch (base)
    {
    case SBO_START_OF_FILE:
        origin = SEEK_SET;
        break;
    case SBO_CURRENT_POSITION:
        origin = SEEK_CUR;
        break;
    case SBO_END_OF_FILE:
        origin = SEEK_END;
        break;
    }

    WSD(stream, fs);
    return _fseeki64(stream->file, offset, origin) == 0;
}

static ssize_t ioWindowsFsGetPosition(FileStream* fs)
{
    WSD(stream, fs);
    ssize_t position = _ftelli64(stream->file);
    if (position < 0)
    {
        LOGF(LogLevel::eERROR, "Error getting seek position in FileStream: %s (%x)", strerror(errno), errno);
    }
    return position;
}

bool ioWindowsFsFlush(FileStream* fs)
{
    WSD(stream, fs);
    if (fflush(stream->file))
    {
        LOGF(LogLevel::eWARNING, "Error flushing system FileStream: %s (%x)", strerror(errno), errno);
        return false;
    }
    return true;
}

bool ioWindowsFsIsAtEnd(FileStream* fs)
{
    WSD(stream, fs);
    return feof(stream->file) != 0;
}

void* ioWindowsGetSystemHandle(FileStream* fs)
{
    WSD(stream, fs);
    return stream->handle;
}

#if defined(XBOX)
#define ioWindowsFsGetResourceMount NULL
#endif

static IFileSystem gWindowsFileIO = {
    ioWindowsFsOpen,
    ioWindowsFsClose,
    ioWindowsFsRead,
    ioWindowsFsWrite,
    ioWindowsFsSeek,
    ioWindowsFsGetPosition,
    ioWindowsFsGetSize,
    ioWindowsFsFlush,
    ioWindowsFsIsAtEnd,
    NULL,
    NULL,
    ioWindowsFsMemoryMap,
    ioWindowsGetSystemHandle,
    NULL,
};

IFileSystem* pSystemFileIO = &gWindowsFileIO;
