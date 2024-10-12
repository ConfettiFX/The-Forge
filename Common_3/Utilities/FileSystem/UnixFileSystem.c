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

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"

extern ResourceDirectoryInfo gResourceDirectories[RD_COUNT];

#if defined(__APPLE__)
#include <sys/param.h>
#endif

struct UnixFileStream
{
    ssize_t size;
    void*   mapping;
    int     descriptor;
};

#define USD(name, fs) struct UnixFileStream* name = (struct UnixFileStream*)(fs)->mUser.data

static const char* getFileName(const struct UnixFileStream* stream, char* buffer, size_t bufferSize)
{
#if defined(__APPLE__)
    char tmpBuffer[MAXPATHLEN];
    if (fcntl(stream->descriptor, F_GETPATH, tmpBuffer) != -1)
    {
        bufferSize = bufferSize > sizeof(tmpBuffer) ? sizeof(tmpBuffer) : bufferSize;
        memcpy(buffer, tmpBuffer, bufferSize);
        buffer[bufferSize - 1] = 0;
        return buffer;
    }
#else
    char fdpath[32];
    snprintf(fdpath, sizeof fdpath, "/proc/self/fd/%i", stream->descriptor);

    ssize_t len = readlink(fdpath, buffer, bufferSize - 1);
    if (len >= 0)
    {
        buffer[len] = 0;
        return buffer;
    }
#endif

    LOGF(eERROR, "Failed to get file name for %i descriptor: %s", stream->descriptor, strerror(errno));
    return "unknown file name";
}

static bool ioUnixFsOpen(IFileSystem* io, const ResourceDirectory rd, const char* fileName, FileMode mode, FileStream* fs)
{
    memset(fs, 0, sizeof *fs);

#if !defined(ANDROID)
    char filePath[FS_MAX_PATH] = { 0 };
    strcat(filePath, gResourceDirectories[rd].mPath);
    strcat(filePath, fileName);
#else
    const char* filePath = fileName;
#endif

    int oflags = 0;

    // 666
    mode_t omode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IROTH;

    if (mode & FM_WRITE)
    {
        oflags |= O_CREAT;

        if (mode & FM_APPEND)
        {
            oflags |= O_APPEND;
        }
        else
        {
            oflags |= O_TRUNC;
        }

        if (mode & FM_READ)
        {
            oflags |= O_RDWR;
        }
        else
        {
            oflags |= O_WRONLY;
        }
    }
    else
    {
        oflags |= O_RDONLY;
    }

    int fd = open(filePath, oflags, omode);
    // Might fail to open the file for read+write if file doesn't exist
    if (fd < 0)
    {
        LOGF(eERROR, "Error opening file '%s': %s", filePath, strerror(errno));
        return false;
    }

    USD(stream, fs);

    stream->size = -1;
    stream->descriptor = fd;

    struct stat finfo;
    if (fstat(stream->descriptor, &finfo) == 0)
    {
        stream->size = finfo.st_size;
    }
    else
    {
        LOGF(eERROR, "Failed to get size for file '%s': %s", filePath, strerror(errno));
    }

    fs->mMode = mode;
    fs->pIO = io;

    if ((mode & FM_READ) && (mode & FM_APPEND) && !(mode & FM_WRITE))
    {
        if (!io->Seek(fs, SBO_END_OF_FILE, 0))
        {
            io->Close(fs);
            return false;
        }
    }

    return true;
}

static bool ioUnixFsMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
{
    *outSize = 0;
    *outData = NULL;

    if (fs->mMode & FM_WRITE)
        return false;

    USD(stream, fs);

    // if not mapped already
    if (!stream->mapping)
    {
        if (stream->size < 0)
            return false;
        if (stream->size == 0)
            return true;

        void* mem = mmap(NULL, (size_t)stream->size, PROT_READ, MAP_PRIVATE, stream->descriptor, 0);

        if (mem == MAP_FAILED)
        {
            char buffer[1024];
            LOGF(eERROR, "mmap is failed for file '%s': %s", getFileName(stream, buffer, sizeof buffer), strerror(errno));
            return false;
        }

        stream->mapping = mem;
    }

    *outSize = (size_t)stream->size;
    *outData = stream->mapping;
    return true;
}

static bool ioUnixFsClose(FileStream* fs)
{
    USD(stream, fs);

    if (stream->mapping)
    {
        if (munmap(stream->mapping, (size_t)stream->size))
        {
            char buffer[1024];
            LOGF(eERROR, "Error unmapping file '%s': '%s'", getFileName(stream, buffer, sizeof buffer), strerror(errno));
        }
        else
        {
            stream->mapping = NULL;
        }
    }

#if defined(FORGE_DEBUG) // we can't get file name after closing it
    char buffer[1024];
    getFileName(stream, buffer, sizeof buffer);
#endif

    bool success = stream->descriptor < 0 || close(stream->descriptor) == 0;
    if (!success)
    {
#if defined(FORGE_DEBUG)
        LOGF(eERROR, "Error after closing file '%s': '%s'", buffer, strerror(errno));
#else
        LOGF(eERROR, "Error after closing file: '%s'", strerror(errno));
#endif
    }
    stream->descriptor = -1;
    return success;
}

static size_t ioUnixFsRead(FileStream* fs, void* dst, size_t size)
{
    USD(stream, fs);
    ssize_t res = read(stream->descriptor, dst, size);
    if (res >= 0)
        return (size_t)res;

    char buffer[1024];
    LOGF(eERROR, "Error reading %s from file '%s': %s", humanReadableSize(size).str, getFileName(stream, buffer, sizeof buffer),
         strerror(errno));
    return 0;
}

static ssize_t ioUnixFsGetPosition(FileStream* fs)
{
    USD(stream, fs);

    off_t res = lseek(stream->descriptor, 0, SEEK_CUR);
    if (res >= 0)
        return res;

    char buffer[1024];
    LOGF(eERROR, "Error getting file position '%s': %s", getFileName(stream, buffer, sizeof buffer), strerror(errno));
    return false;
}

static size_t ioUnixFsWrite(FileStream* fs, const void* src, size_t size)
{
    USD(stream, fs);
    ssize_t res = write(stream->descriptor, src, size);
    if (res >= 0)
        return (size_t)res;

    char buffer[1024];
    LOGF(eERROR, "Error writing %s from file '%s': %s", humanReadableSize(size).str, getFileName(stream, buffer, sizeof buffer),
         strerror(errno));
    return 0;
}

static bool ioUnixFsSeek(FileStream* fs, SeekBaseOffset baseOffset, ssize_t offset)
{
    USD(stream, fs);

    int whence = SEEK_SET;
    switch (baseOffset)
    {
    case SBO_START_OF_FILE:
        whence = SEEK_SET;
        break;
    case SBO_CURRENT_POSITION:
        whence = SEEK_CUR;
        break;
    case SBO_END_OF_FILE:
        whence = SEEK_END;
        break;
    }

    off_t res = lseek(stream->descriptor, offset, whence);
    if (res >= 0)
        return true;

    char buffer[1024];
    LOGF(eERROR, "Error seeking file '%s': %s", getFileName(stream, buffer, sizeof buffer), strerror(errno));
    return false;
}

static bool ioUnixFsFlush(FileStream* fs)
{
    if (!(fs->mMode & FM_WRITE))
        return true;

    USD(stream, fs);

#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
    // datasync is a bit faster, because it can skip flush of modified metadata,
    // e.g. file access time
    if (!fdatasync(stream->descriptor))
        return true;
#else
    if (!fsync(stream->descriptor))
        return true;
#endif

    char buffer[1024];
    LOGF(eERROR, "Failed to flush file '%s': %s", getFileName(stream, buffer, sizeof buffer), strerror(errno));
    return false;
}

static bool unixFsUpdateSize(struct UnixFileStream* stream)
{
    off_t offset = lseek(stream->descriptor, 0, SEEK_CUR);
    if (offset < 0)
        return false;
    off_t size = lseek(stream->descriptor, 0, SEEK_END);
    if (size < 0)
        return false;

    if (offset == size)
    {
        stream->size = size;
        return true;
    }

    off_t offset2 = lseek(stream->descriptor, offset, SEEK_SET);
    if (offset2 < 0 || offset2 != offset)
    {
        char buffer[1024];
        LOGF(eERROR, "File position is broken and so file is closed '%s': %s", getFileName(stream, buffer, sizeof buffer), strerror(errno));
        close(stream->descriptor);
        stream->descriptor = -1;
    }
    stream->size = size;
    return true;
}

static ssize_t ioUnixFsGetSize(FileStream* fs)
{
    USD(stream, fs);
    if ((fs->mMode & FM_WRITE) && !unixFsUpdateSize(stream))
        return -1;
    return stream->size;
}

static void* ioUnixGetSystemHandle(FileStream* fs)
{
    USD(stream, fs);
    return (void*)(ssize_t)stream->descriptor;
}

static bool ioUnixFsIsAtEnd(FileStream* fs) { return ioUnixFsGetPosition(fs) >= ioUnixFsGetSize(fs); }

IFileSystem gUnixSystemFileIO = { ioUnixFsOpen,          ioUnixFsClose, ioUnixFsRead,    ioUnixFsWrite, ioUnixFsSeek, ioUnixFsGetPosition,
                                  ioUnixFsGetSize,       ioUnixFsFlush, ioUnixFsIsAtEnd, NULL,          NULL,         ioUnixFsMemoryMap,
                                  ioUnixGetSystemHandle, NULL };

#if !defined(ANDROID)
IFileSystem* pSystemFileIO = &gUnixSystemFileIO;
#endif
