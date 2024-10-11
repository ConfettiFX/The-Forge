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

#include "../ThirdParty/OpenSource/bstrlib/bstrlib.h"

#include "../../Utilities/Interfaces/IFileSystem.h"
#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ITime.h"

#include "../../Utilities/Interfaces/IMemory.h"

// This macro enables custom ZSTD allocator features
#define ZSTD_STATIC_LINKING_ONLY
#include "../../Utilities/ThirdParty/OpenSource/lz4/lz4.h"
#include "../../Utilities/ThirdParty/OpenSource/zstd/zstd.h"

/************************************************************************/
// MARK: - Filesystem
/************************************************************************/

#define MEMORY_STREAM_GROW_SIZE 4096
#define STREAM_COPY_BUFFER_SIZE 4096
#define STREAM_FIND_BUFFER_SIZE 1024

ResourceDirectoryInfo gResourceDirectories[RD_COUNT] = { { 0 } };

void parse_path_statement(char* PathStatment, size_t size)
{
    char  currentLineStr[1024] = { 0 };
    char* fileCursor = PathStatment;
    char* gGpuDataFileEnd = PathStatment + size;
    // char* previousLineCursor = fileCursor;

    const char* bundledStr = "bundled";
    size_t      bundledStrLen = strlen(bundledStr);
    while (bufferedGetLine(currentLineStr, &fileCursor, gGpuDataFileEnd))
    {
        char*       lineCursor = currentLineStr;
        size_t      ruleLength = strcspn(lineCursor, "#");
        const char* pLineEnd = lineCursor + ruleLength;
        while (currentLineStr != pLineEnd && isspace(*lineCursor))
        {
            ++lineCursor;
        }
        if (lineCursor == pLineEnd)
        {
            continue;
        }

        bool bundled = false;
        if (!memcmp(lineCursor, bundledStr, bundledStrLen))
        {
            lineCursor += bundledStrLen + 1; // 1 for space "bundled RD_XX"
            bundled = true;
        }
        char* resourceName = lineCursor;
        char* split = strchr(lineCursor, ' ');
        if (!split)
        {
            ASSERT(false);
            LOGF(eERROR, "File is not Formated correctly space before/after = is required \n it should be : RD_DIR = PATH");
        }
        size_t resourceNameLen = split - resourceName;

        const char* resourcePath = lineCursor + resourceNameLen + sizeof("= ");
        size_t      resourcePathLen = pLineEnd - resourcePath;
        ASSERT(resourcePathLen + 1 < FS_MAX_PATH);

        ResourceDirectory rd;
        if (!strncmp(resourceName, "RD_SHADER_BINARIES", resourceNameLen))
        {
            rd = RD_SHADER_BINARIES;
        }
        else if (!strncmp(resourceName, "RD_PIPELINE_CACHE", resourceNameLen))
        {
            rd = RD_PIPELINE_CACHE;
        }
        else if (!strncmp(resourceName, "RD_TEXTURES", resourceNameLen))
        {
            rd = RD_TEXTURES;
        }
        else if (!strncmp(resourceName, "RD_COMPILED_MATERIALS", resourceNameLen))
        {
            rd = RD_COMPILED_MATERIALS;
        }
        else if (!strncmp(resourceName, "RD_MESHES", resourceNameLen))
        {
            rd = RD_MESHES;
        }
        else if (!strncmp(resourceName, "RD_FONTS", resourceNameLen))
        {
            rd = RD_FONTS;
        }
        else if (!strncmp(resourceName, "RD_ANIMATIONS", resourceNameLen))
        {
            rd = RD_ANIMATIONS;
        }
        else if (!strncmp(resourceName, "RD_AUDIO", resourceNameLen))
        {
            rd = RD_AUDIO;
        }
        else if (!strncmp(resourceName, "RD_GPU_CONFIG", resourceNameLen))
        {
            rd = RD_GPU_CONFIG;
        }
        else if (!strncmp(resourceName, "RD_LOG", resourceNameLen))
        {
            rd = RD_LOG;
        }
        else if (!strncmp(resourceName, "RD_SCRIPTS", resourceNameLen))
        {
            rd = RD_SCRIPTS;
        }
        else if (!strncmp(resourceName, "RD_SCREENSHOTS", resourceNameLen))
        {
            rd = RD_SCREENSHOTS;
        }
        else if (!strncmp(resourceName, "RD_DEBUG", resourceNameLen))
        {
            rd = RD_DEBUG;
        }
        else if (!strncmp(resourceName, "RD_OTHER_FILES", resourceNameLen))
        {
            rd = RD_OTHER_FILES;
        }
        else
        {
            LOGF(eWARNING, "Unkown Resource Directory in path statement, ignoring ..");
            continue;
        }

        strncpy(gResourceDirectories[rd].mPath, resourcePath, resourcePathLen);
        gResourceDirectories[rd].mBundled = bundled;
    }

    for (uint32_t i = 0; i < RD_COUNT; i++)
    {
        gResourceDirectories[i].pIO = pSystemFileIO;
    }
}

/************************************************************************/
// Memory Stream Functions
/************************************************************************/

struct MemoryStream
{
    uint8_t*    pBuffer;
    uintptr_t   mCursor;
    uintptr_t   mCapacity;
    intptr_t    mSize;
    uintptr_t   mIsOwner;
    FileStream* wrappedStream;
};

#define MEMSD(name, fs) struct MemoryStream* name = (struct MemoryStream*)(fs)->mUser.data

static inline size_t MemoryStreamAvailableSize(struct MemoryStream* stream, size_t requestedSize)
{
    ssize_t sizeLeft = (ssize_t)stream->mSize - (ssize_t)stream->mCursor;
    if (sizeLeft < 0)
        sizeLeft = 0;
    return (ssize_t)requestedSize > sizeLeft ? (size_t)sizeLeft : requestedSize;
}

static bool ioMemoryStreamClose(FileStream* fs)
{
    MEMSD(stream, fs);

    if (stream->mIsOwner)
    {
        tf_free(stream->pBuffer);
    }

    if (stream->wrappedStream)
    {
        fsCloseStream(stream->wrappedStream);
        tf_free(stream->wrappedStream);
    }

    return true;
}

static size_t ioMemoryStreamRead(FileStream* fs, void* dst, size_t size)
{
    if (!(fs->mMode & FM_READ))
    {
        LOGF(eWARNING, "Attempting to read from stream that doesn't have FM_READ flag.");
        return 0;
    }

    MEMSD(stream, fs);

    if ((intptr_t)stream->mCursor >= stream->mSize)
    {
        return 0;
    }

    size_t bytesToRead = MemoryStreamAvailableSize(stream, size);
    memcpy(dst, stream->pBuffer + stream->mCursor, bytesToRead);
    stream->mCursor += bytesToRead;
    return bytesToRead;
}

static size_t ioMemoryStreamWrite(FileStream* fs, const void* src, size_t size)
{
    if (!(fs->mMode & FM_WRITE))
    {
        LOGF(eWARNING, "Attempting to write to stream that doesn't have FM_WRITE flag.");
        return 0;
    }

    MEMSD(stream, fs);

    if (stream->mCursor > (size_t)stream->mSize)
    {
        LOGF(eWARNING, "Creating discontinuity in initialized memory in memory stream.");
    }

    size_t availableCapacity = 0;
    if (stream->mCapacity >= stream->mCursor)
        availableCapacity = stream->mCapacity - stream->mCursor;

    if (size > availableCapacity)
    {
        size_t newCapacity = stream->mCursor + size;

        newCapacity =
            MEMORY_STREAM_GROW_SIZE * (newCapacity / MEMORY_STREAM_GROW_SIZE + (newCapacity % MEMORY_STREAM_GROW_SIZE == 0 ? 0 : 1));

        void* newBuffer = tf_realloc(stream->pBuffer, newCapacity);
        if (!newBuffer)
        {
            LOGF(eERROR,
                 "Failed to reallocate memory stream buffer with new capacity "
                 "%llu.",
                 (unsigned long long)newCapacity);
            return 0;
        }

        stream->pBuffer = (uint8_t*)newBuffer;
        stream->mCapacity = newCapacity;
    }

    memcpy(stream->pBuffer + stream->mCursor, src, size);
    stream->mCursor += size;

    stream->mSize = stream->mSize > (ssize_t)stream->mCursor ? stream->mSize : (ssize_t)stream->mCursor;
    return size;
}

static bool ioMemoryStreamSeek(FileStream* fs, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
    MEMSD(stream, fs);

    switch (baseOffset)
    {
    case SBO_START_OF_FILE:
    {
        if (seekOffset < 0 || seekOffset > stream->mSize)
        {
            return false;
        }
        stream->mCursor = (size_t)seekOffset;
    }
    break;
    case SBO_CURRENT_POSITION:
    {
        ssize_t newPosition = (ssize_t)stream->mCursor + seekOffset;
        if (newPosition < 0 || newPosition > stream->mSize)
        {
            return false;
        }
        stream->mCursor = (size_t)newPosition;
    }
    break;
    case SBO_END_OF_FILE:
    {
        ssize_t newPosition = (ssize_t)stream->mSize + seekOffset;
        if (newPosition < 0 || newPosition > stream->mSize)
        {
            return false;
        }
        stream->mCursor = (size_t)newPosition;
    }
    break;
    }
    return true;
}

static ssize_t ioMemoryStreamGetPosition(FileStream* fs)
{
    MEMSD(stream, fs);
    return (ssize_t)stream->mCursor;
}

static ssize_t ioMemoryStreamGetSize(FileStream* fs)
{
    MEMSD(stream, fs);
    return stream->mSize;
}

static bool ioMemoryStreamFlush(FileStream* fs)
{
    (void)fs;
    // No-op.
    return true;
}

static bool ioMemoryStreamIsAtEnd(FileStream* fs)
{
    MEMSD(stream, fs);
    return (ssize_t)stream->mCursor == stream->mSize;
}

static bool ioMemoryStreamMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
{
    if (fs->mMode & FM_WRITE)
        return false;

    MEMSD(stream, fs);
    *outSize = stream->mCapacity;
    *outData = stream->pBuffer;
    return true;
}

static IFileSystem gMemoryFileIO = {
    NULL,
    ioMemoryStreamClose,
    ioMemoryStreamRead,
    ioMemoryStreamWrite,
    ioMemoryStreamSeek,
    ioMemoryStreamGetPosition,
    ioMemoryStreamGetSize,
    ioMemoryStreamFlush,
    ioMemoryStreamIsAtEnd,
    NULL,
    NULL,
    ioMemoryStreamMemoryMap,
    NULL,
};

/************************************************************************/
// File IO
/************************************************************************/

bool fsIsMemoryStream(FileStream* pStream) { return pStream->pIO == &gMemoryFileIO; }

bool fsIsSystemFileStream(FileStream* pStream) { return pStream->pIO == pSystemFileIO; }

bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* fs)
{
    memset(fs, 0, sizeof *fs);

    size_t size = buffer ? bufferSize : 0;
    size_t capacity = bufferSize;
    // Move cursor to the end for appending buffer
    size_t cursor = (mode & FM_APPEND) ? size : 0;

    // For write streams we have to own the memory as we might need to resize it
    if ((mode & FM_WRITE) && (!owner || !buffer))
    {
        // make capacity multiple of MEMORY_STREAM_GROW_SIZE
        capacity = MEMORY_STREAM_GROW_SIZE * (capacity / MEMORY_STREAM_GROW_SIZE + (capacity % MEMORY_STREAM_GROW_SIZE == 0 ? 0 : 1));
        void* newBuffer = tf_malloc(capacity);
        ASSERT(newBuffer);
        if (buffer)
            memcpy(newBuffer, buffer, size);

        buffer = newBuffer;
        owner = true;
    }

    fs->pIO = &gMemoryFileIO;
    fs->mMode = mode;

    MEMSD(stream, fs);

    stream->pBuffer = (uint8_t*)buffer;
    stream->mCursor = cursor;
    stream->mCapacity = capacity;
    stream->mIsOwner = owner;
    stream->mSize = (ssize_t)size;
    return true;
}

/// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
/// to read from or modify the file. May return NULL if the file could not be opened.
bool fsOpenStreamFromPath(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
    IFileSystem* io = gResourceDirectories[resourceDir].pIO;
    return io->Open(io, resourceDir, fileName, mode, pOut);
}

size_t fsReadBstringFromStream(FileStream* stream, bstring* pStr, size_t symbolsCount)
{
    ASSERT(bisvalid(pStr));

    // Read until the end of the file
    if (symbolsCount == SIZE_MAX)
    {
        bassignliteral(pStr, "");
        int readBytes = 0;
        // read one page at a time
        do
        {
            balloc(pStr, pStr->slen + 512);
            readBytes = (int)fsReadFromStream(stream, pStr->data + pStr->slen, 512);
            ASSERT(INT_MAX - pStr->slen > readBytes && "Integer overflow");
            pStr->slen += readBytes;
        } while (readBytes == 512);
        balloc(pStr, pStr->slen + 1);
        pStr->data[pStr->slen] = '\0';
        return (size_t)pStr->slen;
    }

    ASSERT(symbolsCount < (size_t)INT_MAX);

    bassignliteral(pStr, "");
    balloc(pStr, (int)symbolsCount + 1);
    size_t readBytes = fsReadFromStream(stream, pStr->data, symbolsCount);
    pStr->data[readBytes] = '\0';
    pStr->slen = (int)readBytes;
    return readBytes;
}

bool fsFindStream(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition)
{
    ASSERT(pStream && pFind && pPosition);
    ASSERT(findSize < STREAM_FIND_BUFFER_SIZE);
    ASSERT(maxSeek >= 0);
    if (findSize > (size_t)maxSeek)
        return false;
    if (findSize == 0)
        return true;

    const uint8_t* pattern = (const uint8_t*)pFind;
    // Fill longest proper prefix which is also suffix(lps) array
    uint32_t       lps[STREAM_FIND_BUFFER_SIZE];

    lps[0] = 0;
    for (uint32_t i = 1, prefixLength = 0; i < findSize; ++i)
    {
        while (prefixLength > 0 && pattern[i] != pattern[prefixLength])
        {
            prefixLength = lps[prefixLength - 1];
        }
        if (pattern[i] == pattern[prefixLength])
            ++prefixLength;
        lps[i] = prefixLength;
    }

    size_t patternPos = 0;
    for (; maxSeek != 0; --maxSeek)
    {
        uint8_t byte;
        if (fsReadFromStream(pStream, &byte, sizeof(byte)) != sizeof(byte))
            return false;

        while (true)
        {
            if (byte == pattern[patternPos])
            {
                ++patternPos;
                if (patternPos == findSize)
                {
                    bool result = fsSeekStream(pStream, SBO_CURRENT_POSITION, -(ssize_t)findSize);
                    UNREF_PARAM(result);
                    ASSERT(result);
                    *pPosition = fsGetStreamSeekPosition(pStream);
                    return true;
                }
                break;
            }

            if (patternPos == 0)
                break;

            patternPos = lps[patternPos - 1];
        }
    }
    return false;
}

bool fsFindReverseStream(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition)
{
    ASSERT(pStream && pFind && pPosition);
    ASSERT(findSize < STREAM_FIND_BUFFER_SIZE);
    ASSERT(maxSeek >= 0);
    if (findSize > (size_t)maxSeek)
        return false;
    if (findSize == 0)
        return true;

    const uint8_t* pattern = (const uint8_t*)pFind;
    // Fill longest proper prefix which is also suffix(lps) array
    uint32_t       lps[STREAM_FIND_BUFFER_SIZE];

    lps[findSize - 1] = 0;

    // Doing reverse pass

    for (uint32_t i = (uint32_t)findSize - 1, prefixLength = 0; i-- > 0;)
    {
        uint32_t prefixPos = (uint32_t)findSize - 1 - prefixLength;
        while (prefixLength > 0 && pattern[i] != pattern[prefixPos])
        {
            prefixLength = lps[prefixPos + 1];
            prefixPos = (uint32_t)findSize - 1 - prefixLength;
        }
        if (pattern[i] == pattern[prefixPos])
            ++prefixLength;
        lps[i] = prefixLength;
    }

    size_t patternPos = findSize - 1;
    for (; maxSeek != 0; --maxSeek)
    {
        uint8_t byte;
        if (!fsSeekStream(pStream, SBO_CURRENT_POSITION, -1))
            return false;

        size_t readBytes = fsReadFromStream(pStream, &byte, sizeof(byte));
        ASSERT(readBytes == 1);
        UNREF_PARAM(readBytes);
        fsSeekStream(pStream, SBO_CURRENT_POSITION, -1);

        while (true)
        {
            if (byte == pattern[patternPos])
            {
                if (patternPos-- == 0)
                {
                    *pPosition = fsGetStreamSeekPosition(pStream);
                    return true;
                }
                break;
            }
            else if (patternPos == findSize - 1)
                break;
            else
                patternPos = findSize - 1 - lps[patternPos + 1];
        }
    }
    return false;
}

FORGE_API bool fsStreamWrapMemoryMap(FileStream* fs)
{
    if (fsIsMemoryStream(fs))
        return true;

    size_t      size;
    const void* mem;
    if (!fsStreamMemoryMap(fs, &size, &mem))
        return false;

    FileStream wrapFs;
    if (!fsOpenStreamFromMemory(mem, size, FM_READ, false, &wrapFs))
    {
        // NOTE: fsOpenStreamFromMemory never returns false
        LOGF(eERROR, "Failed to open stream from memory");
        return false;
    }

    MEMSD(stream, &wrapFs);
    stream->mCursor = (size_t)fsGetStreamSeekPosition(fs);

    FileStream* wrappedFs = tf_malloc(sizeof *wrappedFs);
    if (!wrappedFs)
    {
        fsCloseStream(&wrapFs);
        return false;
    }

    memcpy(wrappedFs, fs, sizeof *wrappedFs);
    stream->wrappedStream = wrappedFs;

    *fs = wrapFs;
    return true;
}

FORGE_API void* fsGetSystemHandle(FileStream* fs)
{
    if (!fsIsSystemFileStream(fs))
    {
        ASSERTMSG(false, "Can't have a file handle for a non file stream");
        return NULL;
    }

    return fs->pIO->GetSystemHandle(fs);
}

/************************************************************************/
// Platform independent directory queries
/************************************************************************/

void fsSetPathForResourceDir(IFileSystem* pIO, ResourceDirectory resourceDir, const char* bundledFolder)
{
    ASSERT(pIO);
    ResourceDirectoryInfo* dir = &gResourceDirectories[resourceDir];

    if (dir->mPath[0] != 0)
    {
        LOGF(eWARNING, "Resource directory {%d} already set on:'%s'", resourceDir, dir->mPath);
    }

    strcpy(dir->mPath, bundledFolder);
    dir->pIO = pIO;
}

/************************************************************************/
// MARK: - Archive format definitions
/************************************************************************/

static uint64_t archiveHashMurmur2_64(void const* key, size_t len, uint64_t seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const uint64_t r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len / 8);

    while (data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const uint8_t* data2 = (const uint8_t*)data;

    switch (len & 7)
    {
    // fall through
    case 7:
        h ^= ((uint64_t)data2[6]) << 48;
    // fall through
    case 6:
        h ^= ((uint64_t)data2[5]) << 40;
    // fall through
    case 5:
        h ^= ((uint64_t)data2[4]) << 32;
    // fall through
    case 4:
        h ^= ((uint64_t)data2[3]) << 24;
    // fall through
    case 3:
        h ^= ((uint64_t)data2[2]) << 16;
    // fall through
    case 2:
        h ^= ((uint64_t)data2[1]) << 8;
    // fall through
    case 1:
        h ^= ((uint64_t)data2[0]);
        h *= m;
    }

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

static inline uint64_t archiveKeySlotIndex(uint64_t seed, uint64_t count, const void* key, size_t keySize)
{
    return archiveHashMurmur2_64(key, keySize, seed) % count;
}

struct BucketPointer
{
    uint32_t keyCount;
    uint32_t keyOffset;
};

static int uint64Cmp(const void* v0, const void* v1) { return memcmp(v0, v1, 8); }

struct BunyArHashTable* bunyArHashTableConstruct(uint64_t nodeCount, const struct BunyArNode* nodes, const char* nodeNames)
{
    // More size -> faster computing
    // Values less then 1.5 won't work
    const uint64_t tableSizeMultiplier = 2;

    uint64_t const slotCount = nodeCount * tableSizeMultiplier + 1;
    size_t const   tableSize = slotCount * 8;

    struct BunyArHashTable* ht = tf_calloc(1, sizeof(*ht) + tableSize);

    uint64_t const bucketPointersSize = sizeof(struct BucketPointer) * slotCount;

    uint8_t* const mem = tf_calloc(1, bucketPointersSize + tableSize);

    struct BucketPointer* const buckets = (struct BucketPointer*)mem;

    uint32_t const  keysCount = (uint32_t)(tableSize / 8);
    uint64_t* const keyMem = (uint64_t*)(mem + bucketPointersSize);

    uint64_t* const table = (uint64_t*)(ht + 1);

    ht->tableSlotCount = slotCount;
    for (ht->seed = 0;;)
    {
        uint32_t keysUsed = 0;

        uint32_t maxCollisions = 0;

        uint64_t ni;
        for (ni = 0; ni < nodeCount; ++ni)
        {
            const struct BunyArNode* node = nodes + ni;

            uint64_t index =
                archiveKeySlotIndex(ht->seed, ht->tableSlotCount, nodeNames + node->namePointer.offset, node->namePointer.size);

            table[index] = ni + 1;

            struct BucketPointer* bucket = buckets + index;

            uint32_t nkc = bucket->keyCount + 1;

            if (nkc + keysUsed > keysCount)
                goto NEXT_SEED;

            if (bucket->keyCount)
            {
                memcpy(keyMem + keysUsed, keyMem + bucket->keyOffset, bucket->keyCount * sizeof(*keyMem));
            }

            keyMem[keysUsed + bucket->keyCount] = ni;

            bucket->keyCount = nkc;
            bucket->keyOffset = keysUsed;

            keysUsed += bucket->keyCount;

            if (maxCollisions < bucket->keyCount)
                maxCollisions = bucket->keyCount;
        }

        uint64_t* saltKeys = keyMem + keysUsed;
        keysUsed += maxCollisions;
        uint64_t* saltKeysCopy = keyMem + keysUsed;
        keysUsed += maxCollisions;
        if (keysUsed > keysCount)
            goto NEXT_SEED;

        // Process buckets from most collision to fewest.
        for (uint64_t cc = maxCollisions; cc > 1; --cc)
        {
            for (uint64_t ti = 0; ti < ht->tableSlotCount; ++ti)
            {
                struct BucketPointer* bucket = buckets + ti;

                if (bucket->keyCount != cc)
                    continue;

                // Find a salt value for bucket such that when all items in
                // that bucket are hashed, they claim only unclaimed slots.
                for (uint64_t salt = ht->seed + 1;;)
                {
                    // We can't store salt in more than 63 bits
                    if (salt > ((uint64_t)1 << 63) - 1)
                    {
                        // Also avoids deadloop, although not sure how many
                        // years it takes to get there
                        goto NEXT_SEED;
                    }

                    for (uint32_t ki = 0; ki < bucket->keyCount; ++ki)
                    {
                        ni = keyMem[bucket->keyOffset + ki];

                        const struct BunyArNode* node = nodes + ni;

                        uint64_t index =
                            archiveKeySlotIndex(salt, ht->tableSlotCount, nodeNames + node->namePointer.offset, node->namePointer.size);

                        if (table[index])
                            goto NEXT_SALT; // slot is clamed

                        saltKeys[ki] = index;
                    }

                    // detect duplicated slot claims
                    {
                        memcpy(saltKeysCopy, saltKeys, bucket->keyCount * sizeof(*saltKeys));
                        qsort(saltKeysCopy, bucket->keyCount, sizeof(*saltKeysCopy), uint64Cmp);
                        for (uint32_t ki = 0; ki < bucket->keyCount - 1; ++ki)
                        {
                            if (saltKeysCopy[ki] == saltKeysCopy[ki + 1])
                                goto NEXT_SALT;
                        }
                    }

                    // claim slots
                    for (uint32_t ki = 0; ki < bucket->keyCount; ++ki)
                    {
                        table[saltKeys[ki]] = keyMem[bucket->keyOffset + ki] + 1;
                    }

                    // store salt
                    table[ti] = salt | ((uint64_t)1 << 63);
                    break;
                NEXT_SALT:
                    ++salt;
                }
            }
        }

        // perfect function is ready here
        break;

    NEXT_SEED:
        if (ht->seed == UINT64_MAX)
        {
            // no solution, 18446744073709551615 seeds failed
            // more like joke code path
            tf_free(ht);
            ht = NULL;
            break;
        }

        ++ht->seed;
        memset(table, 0, tableSize);
        memset(buckets, 0, bucketPointersSize);
    }

    tf_free(mem);
    return ht;
}

uint64_t bunyArHashTableLookup(const struct BunyArHashTable* ht, const char* name, uint64_t nodeCount, const struct BunyArNode* nodes,
                               const char* nodeNames)
{
    size_t pathLen = strlen(name);

    uint64_t* table = (uint64_t*)(ht + 1);

    uint64_t index = archiveKeySlotIndex(ht->seed, ht->tableSlotCount, name, pathLen);
    index = table[index];

    if (index >= (uint64_t)1 << 63)
    {
        index = archiveKeySlotIndex(index & (0xffffffffffffffff / 2), ht->tableSlotCount, name, pathLen);
        index = table[index];
    }

    --index;

    if (index >= nodeCount || strcmp(nodeNames + nodes[index].namePointer.offset, name) != 0)
        return UINT64_MAX;
    return index;
}

const char* bunyArFormatName(enum BunyArFileFormat format)
{
    switch (format)
    {
    case BUNYAR_FILE_FORMAT_RAW:
        return "raw";
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
        return "LZ4";
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
        return "zstd";
    default:
        return "unknown";
    }
}

/************************************************************************/
// MARK: - Archive filesystem
/************************************************************************/

struct BunyArBlockBuffer
{
    size_t   usedSize;   // size of valid data
    size_t   memorySize; // buffer limit
    uint8_t* memory;
};

struct BunyArFileStream
{
    struct BunyArNode*             node;
    size_t                         position;
    ZSTD_DCtx*                     zstd_ctx;
    struct BunyArBlockBuffer       compressed;
    struct BunyArBlockBuffer       decompressed;
    BunyArBlockPointer*            currentBlock;
    struct BunyArBlockFormatHeader blocksHeader;
    BunyArBlockPointer*            blocks;
};

struct BunyArMetadata
{
    uint64_t                nodeCount;
    struct BunyArNode*      nodes;
    char*                   nodeNames;
    struct BunyArHashTable* hashTable;

    const uint8_t* memoryBeg;
    const uint8_t* memoryEnd;

    FileStream  ownedStream;
    FileStream* archiveStream;
    uint64_t    virtualStreamCount; // only for validation

    bool  archiveStreamLocking;
    Mutex mutex;
};

struct BunyArNodeSearchCtx
{
    const char* target;
    const char* nodeNames;
};

static void* tfAllocForZstd(void* pUser, size_t size)
{
    (void)pUser;
    return tf_malloc(size);
}
static void tfFreeForZstd(void* pUser, void* memory)
{
    (void)pUser;
    tf_free(memory);
}
static const ZSTD_customMem ZSTD_MEMORY_ALLOCATOR = {
    tfAllocForZstd,
    tfFreeForZstd,
    NULL,
};

static int bunyArNameNodeCmp(const void* v1, const void* v2)
{
    const struct BunyArNodeSearchCtx* ctx = v1;
    const struct BunyArNode*          node = v2;
    return strcmp(ctx->target, ctx->nodeNames + node->namePointer.offset);
}

static void initBunyArFsInterface(IFileSystem*, struct BunyArMetadata*);

static inline struct BunyArMetadata* getFsArchive(IFileSystem* fs) { return (struct BunyArMetadata*)fs->pUser; }

static inline struct BunyArFileStream* getFsBunyArStream(FileStream* fs) { return (struct BunyArFileStream*)fs->mUser.data[0]; }

static inline void bunyArMemoryReadPrepare(const struct BunyArMetadata* a,
                                           const struct BunyArPointer64 ptr, //-V801
                                           uint8_t const** out, uint64_t* read)
{
    *out = a->memoryBeg + ptr.offset;
    *read = (uint64_t)(a->memoryEnd - *out);
    if (*read > ptr.size)
        *read = ptr.size;
}

static inline size_t bunyArStreamRead(struct BunyArMetadata* a, uint64_t position, uint64_t size, void* dst)
{
    if (a->memoryBeg)
    {
        const uint8_t* cur;
        uint64_t       read;
        bunyArMemoryReadPrepare(a, (struct BunyArPointer64){ position, size }, &cur, &read);
        memcpy(dst, cur, read);
        return read;
    }

    if (a->archiveStreamLocking)
        acquireMutex(&a->mutex);

    size_t readed = 0;
    if (fsGetStreamSeekPosition(a->archiveStream) != (ssize_t)position &&
        !fsSeekStream(a->archiveStream, SBO_START_OF_FILE, (ssize_t)position))
    {
        readed = 0;
    }
    else
    {
        readed = fsReadFromStream(a->archiveStream, dst, size);
    }

    if (a->archiveStreamLocking)
        releaseMutex(&a->mutex);

    return readed;
}

static inline bool bunyArReadLocation(struct BunyArMetadata* a,
                                      struct BunyArPointer64 ptr, //-V813
                                      void*                  dst)
{
    return bunyArStreamRead(a, ptr.offset, ptr.size, dst) == ptr.size;
}

static const struct ArchiveOpenDesc BUNYAR_OPEN_DESC_DEFAULT = { 0 };

static bool bunyArchiveOpen(FileStream* stream, uint64_t memorySize, const void* memory, const struct ArchiveOpenDesc* desc,
                            IFileSystem* out)
{
    ASSERT((stream == NULL) != (memory == NULL));
    memset(out, 0, sizeof *out);

    if (!desc)
        desc = &BUNYAR_OPEN_DESC_DEFAULT;

    bool streamMode = stream != NULL;

    ////////////////////////
    // Read and check header

    struct BunyArHeader header;

    bool headerReaded = false;

    if (streamMode)
    {
        headerReaded = fsSeekStream(stream, SBO_START_OF_FILE, 0) && fsReadFromStream(stream, &header, sizeof header) == sizeof header;
    }
    else if (memorySize >= sizeof header)
    {
        memcpy(&header, memory, sizeof header);
        headerReaded = true;
    }

    if (!headerReaded)
    {
        if (desc->tryMode)
            return false;
        LOGF(eERROR, "Failed to open archive: failed to read header");
        return false;
    }

    if (memcmp(&header.magic, &BUNYAR_MAGIC, sizeof(header.magic)) != 0) //-V614
    {
        if (desc->tryMode)
            return false;
        LOGF(eERROR, "Failed to open archive: wrong magic value %llu, expected %llu", (unsigned long long)header.magic,
             (unsigned long long)(*((const uint64_t*)&BUNYAR_MAGIC)));
        return false;
    }

    if (header.version.compatible != 0)
    {
        LOGF(eERROR, "Failed to open archive: version %llu not supported, expected 0", (unsigned long long)header.version.actual);
        return false;
    }

    ///////////////////////////////////////
    // Allocate memory for archive metadata
    // includes Archive struct, file nodes, file names

    struct BunyArMetadata* archive =
        (struct BunyArMetadata*)tf_malloc(sizeof(*archive) + header.nodesPointer.size + header.namesPointer.size);

    {
        uint8_t* memPtr = (uint8_t*)(archive + 1);

        memset(archive, 0, sizeof(*archive));

        archive->memoryBeg = (const uint8_t*)memory;
        archive->memoryEnd = archive->memoryBeg + memorySize;

        archive->archiveStream = stream;

        archive->nodeCount = header.nodesPointer.size / sizeof(struct BunyArNode);
        archive->nodes = (struct BunyArNode*)memPtr;
        memPtr += header.nodesPointer.size;

        archive->nodeNames = (char*)memPtr;
        memPtr += header.namesPointer.size;
    }

    /////////////
    // Read nodes

    if (!bunyArReadLocation(archive, header.nodesPointer, archive->nodes))
    {
        LOGF(eERROR, "Failed to open archive: nodes reading failure");
        goto CANCEL;
    }

    for (uint64_t fi = 0; fi < archive->nodeCount; ++fi)
    {
        struct BunyArNode* node = archive->nodes + fi;

        if (node->namePointer.offset + node->namePointer.size > header.namesPointer.size)
        {
            LOGF(eERROR, "Failed to open archive: invalid namePointer for node %llu", (unsigned long long)fi);
            goto CANCEL;
        }
    }

    //////////////////
    // Read node names

    if (header.namesPointer.size)
    {
        if (!bunyArReadLocation(archive, header.namesPointer, archive->nodeNames))
        {
            LOGF(eERROR, "Failed to open archive: unexpected end of stream");
            goto CANCEL;
        }

        if (archive->nodeNames[header.namesPointer.size - 1] != 0)
        {
            LOGF(eERROR, "Failed to open archive: missing 0 in the string end");
            goto CANCEL;
        }
    }
    else if (archive->nodeCount)
    {
        LOGF(eERROR, "Failed to open archive: missing node names");
    CANCEL:
        tf_free(archive);
        return false;
    }

    //////////////////
    // Read hash table

    if (!desc->disableHashTable && header.hashTablePointer.size)
    {
        archive->hashTable = tf_malloc(header.hashTablePointer.size);

        if (!bunyArReadLocation(archive, header.hashTablePointer, archive->hashTable))
        {
            // not fatal, we can recreate it
            LOGF(eERROR, "Failed to read archive hash table");
            tf_free(archive->hashTable);
            archive->hashTable = NULL;
        }
    }

    //////////////////////////
    /// Validation/fixes phase

    bool validation = desc->validation;
#if defined(FORGE_DEBUG)
    validation = true;
#endif

    if (validation) // check ArchiveBlockPointer how packed/unpacked  //-V547
    {
        const struct BunyArBlockInfo info = { 1, 345, 432 };

        BunyArBlockPointer ptr;

        bool test = bunyArEncodeBlockPointer(info, &ptr) && bunyArDecodeBlockPointer(ptr).size == info.size &&
                    bunyArDecodeBlockPointer(ptr).offset == info.offset && bunyArDecodeBlockPointer(ptr).isCompressed == info.isCompressed;
        if (!VERIFYMSG(test, "Archive block pointer read/write test failure"))
        {
            goto CANCEL;
        }
    }

    if (validation) // detect and fix unnormalized paths  //-V547
    {
        bool namesChanged = false;

        for (uint64_t i = 0; i < archive->nodeCount; ++i)
        {
            struct BunyArNode* node = archive->nodes + i;

            char* name = archive->nodeNames + node->namePointer.offset;

            size_t strLen = strlen(name);
            if (node->namePointer.size > BUNYAR_FILE_NAME_LENGTH_MAX || node->namePointer.size != strLen)
            {
                LOGF(eERROR,
                     "Archive contains file with wrong name length %llu when "
                     "limit is %i and node name size is %llu: '%s'",
                     (unsigned long long)strLen, BUNYAR_FILE_NAME_LENGTH_MAX, (unsigned long long)node->namePointer.size, name);

                node->namePointer.size = (uint32_t)strLen;

                namesChanged = true;
            }
        }

        if (namesChanged)
        {
            LOGF(eERROR, "Baked archive hash table is abandoned because node names was "
                         "modified");
            tf_free(archive->hashTable);
            archive->hashTable = NULL;
        }
    }

    if (validation && archive->hashTable)
    {
        bool success = true;
        for (size_t i = 0; i < archive->nodeCount; ++i)
        {
            uint64_t value = bunyArHashTableLookup(archive->hashTable, archive->nodeNames + archive->nodes[i].namePointer.offset,
                                                   archive->nodeCount, archive->nodes, archive->nodeNames);

            if (value >= archive->nodeCount)
            {
                LOGF(eERROR, "Archive hash table lookup test failed: key wasn't found.");
                success = false;
                break;
            }

            if (value != i)
            {
                LOGF(eERROR, "Archive hash table lookup test failed: got the wrong key.");
                success = false;
                break;
            }
        }

        if (!success)
        {
            LOGF(eERROR, "Baked archive hash table is abandoned because it was faulty");
            tf_free(archive->hashTable);
            archive->hashTable = NULL;
        }
    }

    ////////////////////////////////////
    // Initialize hash table if required
    // binary search is used in case hash table is not initialized

    if (!desc->disableHashTable && !archive->hashTable)
    {
        Timer timer;
        initTimer(&timer);

        archive->hashTable = bunyArHashTableConstruct(archive->nodeCount, archive->nodes, archive->nodeNames);

        uint32_t msec = getTimerMSec(&timer, false);

        if (msec)
        {
            LOGF(eINFO, "Archive hash table %s in %fs", archive->hashTable ? "constructed" : "construction is failed",
                 (double)(msec) / 1000);
        }
    }

    /////////////////////////////////////////////
    // Archive preparations are done, fill output

    initBunyArFsInterface(out, archive);

    if (streamMode && desc->protectStreamCriticalSection)
    {
        if (!initMutex(&archive->mutex))
        {
            fsArchiveClose(out);
            return false;
        }

        archive->archiveStreamLocking = true;
    }

    return true;
}

bool fsArchiveOpen(ResourceDirectory rd, const char* path, const struct ArchiveOpenDesc* desc, IFileSystem* out)
{
    FileStream stream;
    if (!fsOpenStreamFromPath(rd, path, FM_READ, &stream))
        return false;

    if (!fsArchiveOpenFromStream(&stream, desc, out))
    {
        if (!desc->tryMode)
        {
            LOGF(eERROR, "Failed to open archive: \'%s\'", path);
        }
        fsCloseStream(&stream);
        return false;
    }

    struct BunyArMetadata* archive = getFsArchive(out);

    archive->ownedStream = stream;
    archive->archiveStream = &archive->ownedStream;

    return true;
}

bool fsArchiveOpenFromStream(FileStream* stream, const struct ArchiveOpenDesc* desc, IFileSystem* out)
{
    if (desc->mmap || fsIsMemoryStream(stream))
    {
        size_t      msize;
        const void* m;
        if (fsStreamMemoryMap(stream, &msize, &m))
            return bunyArchiveOpen(NULL, msize, m, desc, out);
    }
    return bunyArchiveOpen(stream, 0, NULL, desc, out);
}

FORGE_API bool fsArchiveOpenFromMemory(uint64_t msize, const void* m, const struct ArchiveOpenDesc* desc, IFileSystem* out)
{
    return bunyArchiveOpen(NULL, msize, m, desc, out);
}

FORGE_API bool fsArchiveClose(IFileSystem* fs)
{
    if (!fs || !fs->pUser)
        return true;

    struct BunyArMetadata* archive = getFsArchive(fs);

    if (archive->virtualStreamCount > 0)
    {
        LOGF(eERROR, "Archive closed while some files are still opened");
    }

    if (archive->ownedStream.pIO)
    {
        fsCloseStream(&archive->ownedStream);
    }

    if (archive->archiveStreamLocking)
    {
        exitMutex(&archive->mutex);
    }

    tf_free(archive->hashTable);
    tf_free(archive);
    return true;
}

bool fsArchiveGetNodeId(IFileSystem* fs, const char* fileName, uint64_t* outUid)
{
    struct BunyArMetadata* archive = getFsArchive(fs);

    if (archive->hashTable)
    {
        *outUid = bunyArHashTableLookup(archive->hashTable, fileName, archive->nodeCount, archive->nodes, archive->nodeNames);
    }
    else
    {
        struct BunyArNodeSearchCtx ctx = { fileName, archive->nodeNames };

        struct BunyArNode* node = (struct BunyArNode*)bsearch(&ctx, archive->nodes, archive->nodeCount, sizeof(*node), bunyArNameNodeCmp);
        if (!node)
            return false;

        *outUid = (uint64_t)(node - archive->nodes);
    }

    return *outUid != UINT64_MAX;
}

static bool ioArchiveGetFileUid(IFileSystem* fs, ResourceDirectory rd, const char* fileName, uint64_t* outUid)
{
    char path[BUNYAR_FILE_NAME_LENGTH_MAX + 1] = { 0 };
    strcat(path, gResourceDirectories[rd].mPath);
    strcat(path, fileName);
    return fsArchiveGetNodeId(fs, path, outUid);
}

static bool ioArchiveOpenByUid(IFileSystem* inFs, uint64_t index, FileMode mode, FileStream* pOutStream)
{
    memset(pOutStream, 0, sizeof *pOutStream);

    struct BunyArMetadata* archive = getFsArchive(inFs);

    if (index > archive->nodeCount)
    {
        LOGF(eERROR, "Cannot open archive file by UID %llu: bad UID", (unsigned long long)index);
        return false;
    }

    struct BunyArNode* node = &archive->nodes[index];

    if (mode != FM_READ)
    {
        LOGF(eERROR, "Cannot open archive file '%s': only FM_READ is supported", archive->nodeNames + node->namePointer.offset);
        return false;
    }

    size_t compressedBufferSize = 0;
    size_t decompressedBufferSize = 0;

    struct BunyArBlockFormatHeader blocksHeader = { 0 };

    switch (node->format)
    {
    case BUNYAR_FILE_FORMAT_RAW:
    {
        if (node->originalFileSize == node->filePointer.size)
            break;

        LOGF(eERROR, "Archive contains raw file '%s' with corrupted size (%llu vs %llu)", archive->nodeNames + node->namePointer.offset,
             (unsigned long long)node->originalFileSize, (unsigned long long)node->filePointer.size);
        return false;
    }
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
    {
        if (node->filePointer.size < sizeof(blocksHeader))
        {
            LOGF(eERROR, "Currupted archive file '%s'", archive->nodeNames + node->namePointer.offset);
            return false;
        }

        if (bunyArStreamRead(archive, node->filePointer.offset, sizeof(blocksHeader), &blocksHeader) != sizeof(blocksHeader))
            return false;

        if (blocksHeader.blockSize == 0)
        {
            LOGF(eERROR, "Archive file '%s' is stored with 0 block size", archive->nodeNames + node->namePointer.offset);
            return false;
        }

        decompressedBufferSize = blocksHeader.blockSize;
        compressedBufferSize = archive->memoryBeg ? 0 : blocksHeader.blockSize;
    }
    break;
    default:
    {
        LOGF(eERROR, "Archive contains file '%s' written with unsupported format %llu", archive->nodeNames + node->namePointer.offset,
             (unsigned long long)node->format);
        return false;
    }
    }

    size_t blocksSize = blocksHeader.blockCount * sizeof(BunyArBlockPointer);

    struct BunyArFileStream* fs =
        (struct BunyArFileStream*)tf_malloc(sizeof(*fs) + compressedBufferSize + decompressedBufferSize + blocksSize);

    memset(fs, 0, sizeof(*fs));

    fs->node = node;
    fs->blocksHeader = blocksHeader;
    fs->blocks = (BunyArBlockPointer*)(fs + 1);
    fs->compressed.memory = (uint8_t*)(fs->blocks) + blocksSize;
    fs->compressed.memorySize = compressedBufferSize;
    fs->decompressed.memory = fs->compressed.memory + compressedBufferSize;
    fs->decompressed.memorySize = decompressedBufferSize;

    if (blocksSize && //
        !bunyArStreamRead(archive, node->filePointer.offset + sizeof(blocksHeader), blocksSize, fs->blocks))
    {
    CANCEL:
        tf_free(fs);
        return false;
    }

    switch (node->format)
    {
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
    {
        fs->zstd_ctx = ZSTD_createDCtx_advanced(ZSTD_MEMORY_ALLOCATOR);
        if (!fs->zstd_ctx)
        {
            LOGF(eERROR, "Failed to create ZSTD decompression context");
            goto CANCEL;
        }
    }
    }

    pOutStream->pIO = inFs;
    pOutStream->mMode = mode;

    pOutStream->mUser.data[0] = (uintptr_t)fs;

    ++archive->virtualStreamCount;

    return true;
}

static bool ioArchiveFsOpen(IFileSystem* fs, const ResourceDirectory rd, const char* fileName, FileMode mode, FileStream* pOutStream)
{
    uint64_t index;
    if (!fs->GetFileUid(fs, rd, fileName, &index))
        return false;
    return fs->OpenByUid(fs, index, mode, pOutStream);
}

static bool ioArchiveFsClose(FileStream* fs)
{
    if (!fs->pIO)
        return true;

    ASSERT(fs->mUser.data[0]);

    struct BunyArMetadata* archive = getFsArchive(fs->pIO);
    ASSERT(archive->virtualStreamCount != 0);
    --archive->virtualStreamCount;

    struct BunyArFileStream* stream = getFsBunyArStream(fs);
    ZSTD_freeDCtx(stream->zstd_ctx);
    tf_free(stream);

    memset(fs, 0, sizeof *fs);
    return true;
}

static inline struct BunyArPointer64 bunyArDecodeBlockPointerInfo(struct BunyArNode* node, struct BunyArBlockFormatHeader* blockHeader,
                                                                  struct BunyArBlockInfo* block)
{
    return (struct BunyArPointer64){
        node->filePointer.offset + sizeof(*blockHeader) + sizeof(BunyArBlockPointer) * blockHeader->blockCount + block->offset,
        block->size,
    };
}

static bool bunyArReadBlockToBuffer(struct BunyArMetadata* archive, struct BunyArFileStream* fs, BunyArBlockPointer* blockToRead,
                                    struct BunyArBlockBuffer* dst)
{
    uint64_t       srcSize;
    const uint8_t* srcMemory;

    {
        struct BunyArBlockInfo blockInfo = bunyArDecodeBlockPointer(*blockToRead);

        ASSERT(blockInfo.isCompressed);

        struct BunyArPointer64 loc = bunyArDecodeBlockPointerInfo(fs->node, &fs->blocksHeader, &blockInfo);

        if (archive->memoryBeg)
        {
            bunyArMemoryReadPrepare(archive, loc, &srcMemory, &srcSize);
            if (srcSize != loc.size)
                return false;
        }
        else
        {
            if (!bunyArReadLocation(archive, loc, fs->compressed.memory))
                return false;
            srcMemory = fs->compressed.memory;
            srcSize = loc.size;
        }
    }

    const char* error = NULL;

    // this function fills readSize and writeSize
    switch (fs->node->format)
    {
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    {
        int decompressedSize = LZ4_decompress_safe((const char*)srcMemory, (char*)dst->memory, (int)srcSize, (int)dst->memorySize);

        if (decompressedSize < 0)
        {
            error = "compressed data is corrupted";
            break;
        }

        dst->usedSize = (size_t)decompressedSize;
    }
    break;
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
    {
        size_t decompressedSize = ZSTD_decompressDCtx(fs->zstd_ctx, dst->memory, dst->memorySize, srcMemory, srcSize);

        if (ZSTD_isError(decompressedSize))
        {
            error = ZSTD_getErrorName(decompressedSize);
            break;
        }

        dst->usedSize = decompressedSize;
    }
    break;
    default:
        error = "Unexpected node format";
    }

    if (!error)
        return true;

    LOGF(eERROR, "Failed to decompress block #%llu: %s", (unsigned long long)(blockToRead - fs->blocks), error);
    return false;
}

static bool bunyArReadBlockToStagingBuffer(struct BunyArMetadata* archive, struct BunyArFileStream* fs, BunyArBlockPointer* blockToRead)
{
    if (fs->currentBlock == blockToRead)
        return true;

    if (bunyArReadBlockToBuffer(archive, fs, blockToRead, &fs->decompressed))
    {
        fs->currentBlock = blockToRead;
        return true;
    }

    // decompressed buffer data is corrupted, so we have to reset handle
    fs->currentBlock = NULL;
    return false;
}

static size_t ioArchiveFsRead(FileStream* pFile, void* outputBuffer, size_t outputSize)
{
    struct BunyArFileStream* fs = getFsBunyArStream(pFile);
    struct BunyArMetadata*   archive = getFsArchive(pFile->pIO);
    struct BunyArNode*       node = fs->node;

    switch (node->format)
    {
    case BUNYAR_FILE_FORMAT_RAW:
    {
        size_t limit = node->filePointer.size - fs->position;
        if (outputSize > limit)
            outputSize = limit;

        size_t readSize = bunyArStreamRead(archive, node->filePointer.offset + fs->position, outputSize, outputBuffer);
        fs->position += readSize;
        return readSize;
    }
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
    {
        uint8_t* dstMemory = (uint8_t*)outputBuffer;
        size_t   sizeToWrite = outputSize;

        while (sizeToWrite > 0)
        {
            uint64_t blockIndex = fs->position / fs->blocksHeader.blockSize;

            if (blockIndex >= fs->blocksHeader.blockCount)
                break;

            uint64_t offsetInBlock = fs->position - blockIndex * fs->blocksHeader.blockSize;

            uint64_t blockSize =
                blockIndex == fs->blocksHeader.blockCount - 1 ? fs->blocksHeader.blockSizeLast : fs->blocksHeader.blockSize;

            // reached end of file
            if (offsetInBlock >= blockSize)
                break;

            BunyArBlockPointer* block = fs->blocks + blockIndex;

            struct BunyArBlockInfo blockInfo = bunyArDecodeBlockPointer(*block);

            uint64_t sizeDone = 0;

            if (!blockInfo.isCompressed)
            {
                // Block is uncompressed, read it directly

                if (blockSize != blockInfo.size)
                {
                    LOGF(eERROR, "Raw black size is wrong (%llu vs %llu)", (unsigned long long)blockSize,
                         (unsigned long long)blockInfo.size);
                    break;
                }

                struct BunyArPointer64 location = bunyArDecodeBlockPointerInfo(node, &fs->blocksHeader, &blockInfo);

                size_t sizeLeft = location.size - offsetInBlock;

                ASSERT(sizeLeft);

                sizeDone =
                    bunyArStreamRead(archive, location.offset + offsetInBlock, sizeToWrite > sizeLeft ? sizeLeft : sizeToWrite, dstMemory);
            }
            else if (offsetInBlock == 0 && sizeToWrite >= blockSize)
            {
                // Avoid usage of staging buffer.
                // We can uncompress entire block to user memory.

                struct BunyArBlockBuffer buffer = { 0 };

                buffer.memory = dstMemory;
                buffer.memorySize = sizeToWrite;
                bunyArReadBlockToBuffer(archive, fs, block, &buffer);

                sizeDone = buffer.usedSize;
            }
            else
            {
                // Decompress block to staging buffer.
                // Stream from it by requested pieces.

                if (fs->currentBlock != block && !bunyArReadBlockToStagingBuffer(archive, fs, block))
                    break;

                // We can't reach end of last block here, because of test before
                ASSERT(fs->decompressed.usedSize > offsetInBlock);

                // copy data from decompressed buffer to user memory
                uint64_t availableSize = fs->decompressed.usedSize - offsetInBlock;

                sizeDone = availableSize > sizeToWrite ? sizeToWrite : availableSize;

                memcpy(dstMemory, fs->decompressed.memory + offsetInBlock, sizeDone);
            }

            dstMemory += sizeDone;
            sizeToWrite -= sizeDone;
            fs->position += sizeDone;
        }

        return outputSize - sizeToWrite;
    }
    default:
        // we can't be here, because format is checked on file opening
        return 0;
    }
}

static bool ioArchiveFsSeek(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
    struct BunyArFileStream* stream = getFsBunyArStream(pFile);

    ssize_t resultPos = seekOffset;

    switch (baseOffset)
    {
    case SBO_START_OF_FILE:
        break;
    case SBO_CURRENT_POSITION:
        resultPos += (ssize_t)stream->position;
        break;
    case SBO_END_OF_FILE:
        resultPos += (ssize_t)stream->node->originalFileSize;
        break;
    default:
        return false;
    }

    if (resultPos < 0 || resultPos > (ssize_t)stream->node->originalFileSize)
        return false;

    stream->position = (size_t)resultPos;
    return true;
}

static ssize_t ioArchiveFsGetSeekPosition(FileStream* pFile) { return (ssize_t)getFsBunyArStream((FileStream*)(uintptr_t)pFile)->position; }

static ssize_t ioArchiveFsGetFileSize(FileStream* fs)
{
    struct BunyArFileStream* stream = getFsBunyArStream(fs);
    return (ssize_t)stream->node->originalFileSize;
}

static bool ioArchiveFsIsAtEnd(FileStream* pFile)
{
    struct BunyArFileStream* stream = getFsBunyArStream((FileStream*)(uintptr_t)pFile);

    return stream->position >= stream->node->originalFileSize;
}

static bool ioArchiveMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
{
    *outSize = 0;
    *outData = NULL;

    struct BunyArFileStream* stream = getFsBunyArStream((FileStream*)(uintptr_t)fs);

    struct BunyArMetadata* archive = getFsArchive(fs->pIO);
    if (!archive->memoryBeg)
        return false;

    struct BunyArNode* node = stream->node;
    if (node->format != BUNYAR_FILE_FORMAT_RAW)
        return false;

    *outSize = node->filePointer.size;
    *outData = archive->memoryBeg + node->filePointer.offset;
    return true;
}

static void initBunyArFsInterface(IFileSystem* fs, struct BunyArMetadata* archive)
{
    memset(fs, 0, sizeof *fs);

    fs->Open = ioArchiveFsOpen;
    fs->Close = ioArchiveFsClose;
    fs->Read = ioArchiveFsRead;
    fs->Write = NULL;
    fs->Seek = ioArchiveFsSeek;
    fs->GetSeekPosition = ioArchiveFsGetSeekPosition;
    fs->GetFileSize = ioArchiveFsGetFileSize;
    fs->Flush = NULL;
    fs->IsAtEnd = ioArchiveFsIsAtEnd;
    fs->GetFileUid = ioArchiveGetFileUid;
    fs->OpenByUid = ioArchiveOpenByUid;
    fs->MemoryMap = ioArchiveMemoryMap;

    fs->pUser = archive;
}

/************************************************************************/
// MARK: - Advanced Archive filesystem IO
/************************************************************************/

void fsArchiveGetDescription(IFileSystem* fs, struct BunyArDescription* outInfo)
{
    memset(outInfo, 0, sizeof *outInfo);

    struct BunyArMetadata* archive = getFsArchive(fs);

    outInfo->nodeCount = archive->nodeCount;
    outInfo->hashTable = archive->hashTable;
}

bool fsArchiveGetNodeDescription(IFileSystem* fs, uint64_t nodeId, struct BunyArNodeDescription* outInfo)
{
    memset(outInfo, 0, sizeof *outInfo);

    struct BunyArMetadata* archive = getFsArchive(fs);

    if (nodeId >= archive->nodeCount)
        return false;

    struct BunyArNode* node = archive->nodes + nodeId;

    outInfo->name = archive->nodeNames + node->namePointer.offset;
    outInfo->fileSize = node->originalFileSize;
    outInfo->compressedSize = node->filePointer.size;
    outInfo->format = (enum BunyArFileFormat)node->format;

    return true;
}

bool fsArchiveGetFileBlockMetadata(FileStream* pFile, struct BunyArBlockFormatHeader* outHeader, const BunyArBlockPointer** outBlockPtrs)
{
    if (!pFile)
        return false;

    struct BunyArFileStream* stream = getFsBunyArStream((FileStream*)(uintptr_t)pFile);

    *outHeader = stream->blocksHeader;
    *outBlockPtrs = stream->blocks;
    return *outBlockPtrs != NULL;
}

/************************************************************************/
/************************************************************************/
