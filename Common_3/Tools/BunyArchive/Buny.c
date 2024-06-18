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

#define BUNYAR_LIB_INTERNAL
#include "Buny.h"

#include "../../Utilities/ThirdParty/OpenSource/Nothings/stb_ds.h"

#include "../../Utilities/Interfaces/ILog.h"
#include "../../Utilities/Interfaces/IThread.h"
#include "../../Utilities/Interfaces/ITime.h"

#include "../../Utilities/Threading/Atomics.h"
#include "../../Utilities/Threading/ThreadSystem.h"

#include "utf8.h"

// This macro enables custom ZSTD allocator features
#define ZSTD_STATIC_LINKING_ONLY
#include "../../Utilities/ThirdParty/OpenSource/lz4/lz4hc.h"
#include "../../Utilities/ThirdParty/OpenSource/zstd/zstd.h"
#include "../../Utilities/ThirdParty/OpenSource/zstd/zstd_errors.h"

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibCreate (part one)                                     ///
/// First step.                                                              ///
/// Preprocessing - converting entries to files with validation              ///
////////////////////////////////////////////////////////////////////////////////

// Clamping clevel has no effect, but I use this for warning logs.
static void validateCompressionLevel(enum BunyArFileFormat format, int* cl)
{
    switch (format)
    {
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    {
        if (*cl == BUNYAR_LIB_COMPRESSION_LEVEL_DEFAULT)
        {
            *cl = LZ4HC_CLEVEL_DEFAULT;
        }
        else if (*cl <= 0)
        {
            int accelerationLevel = -*cl;

            // from lz4.c
            const int LZ4_ACCELERATION_MAX = 65537;

            if (accelerationLevel > LZ4_ACCELERATION_MAX)
            {
                LOGF(eWARNING, "Acceleration level for LZ4 is out of bounds. (%i > %i max)", accelerationLevel, LZ4_ACCELERATION_MAX);
                accelerationLevel = LZ4_ACCELERATION_MAX;

                *cl = -accelerationLevel;
            }
        }
        else if (*cl < LZ4HC_CLEVEL_MIN)
        {
            LOGF(eWARNING, "Compression level for LZ4HC is out of bounds. (%i < %i min)", *cl, LZ4HC_CLEVEL_MIN);
            *cl = LZ4HC_CLEVEL_MIN;
        }
        else if (*cl > LZ4HC_CLEVEL_MAX)
        {
            LOGF(eWARNING, "Compression level for LZ4HC is out of bounds. (%i > %i max)", *cl, LZ4HC_CLEVEL_MAX);
            *cl = LZ4HC_CLEVEL_MAX;
        }
    }
    break;
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
    {
        if (*cl == BUNYAR_LIB_COMPRESSION_LEVEL_DEFAULT)
        {
            *cl = ZSTD_defaultCLevel();
            break;
        }

        int mincl = ZSTD_minCLevel();
        int maxcl = ZSTD_maxCLevel();

        if (*cl < mincl)
        {
            LOGF(eWARNING, "Compression level for ZSTD is out of bounds. (%i < %i min)", *cl, mincl);
            *cl = mincl;
        }
        else if (*cl > maxcl)
        {
            LOGF(eWARNING, "Compression level for ZSTD is out of bounds. (%i > %i max)", *cl, maxcl);
            *cl = maxcl;
        }
    }
    break;
    case BUNYAR_FILE_FORMAT_RAW:
    default:
        break;
    }
}

static const char* bunyArLibMergePath(char*** strings, const char* dir, const char* path, bool reusePath)
{
    size_t dirLength = strlen(dir);
    bool   emptydir = dirLength == 0 || (dirLength == 1 && *dir == '.');
    if (reusePath && emptydir)
        return path;

    size_t entryLength = strlen(path);

    char* str = NULL;
    if (emptydir)
    {
        str = (char*)tf_malloc(entryLength + 1);
        memcpy(str, path, entryLength + 1);
    }
    else
    {
        str = (char*)tf_malloc(dirLength + 1 + entryLength + 1);
        memcpy(str, dir, dirLength);
        str[dirLength] = '/';
        memcpy(str + dirLength + 1, path, entryLength + 1);
    }

    arrpush(*strings, str);
    return str;
}

static bool bunyArLibCreateUnfoldDirectory(const struct BunyArLibEntryCreateDesc* entryReference, uint64_t* entryCount,
                                           struct BunyArLibEntryCreateDesc** entries, char*** strings, const char* srcDir,
                                           const char* dstDir)
{
    FsDirectoryIterator iterator = NULL;
    if (!fsDirectoryIteratorOpen(entryReference->inputRd, srcDir, &iterator))
    {
        LOGF(eERROR, "Failed to open directory '%s'", srcDir);
        return false;
    }

    bool success = true;
    for (;;)
    {
        struct FsDirectoryIteratorEntry entry;
        if (!fsDirectoryIteratorNext(iterator, &entry))
        {
            success = false;
            break;
        }

        if (!entry.name)
            break;

        const char* srcStr = bunyArLibMergePath(strings, srcDir, entry.name, entry.isDir);
        const char* dstStr = srcDir == dstDir ? srcStr : bunyArLibMergePath(strings, dstDir, entry.name, entry.isDir);

        if (entry.isDir)
        {
            if (!bunyArLibCreateUnfoldDirectory(entryReference, entryCount, entries, strings, srcStr, dstStr))
            {
                success = false;
                break;
            }
        }
        else
        {
            struct BunyArLibEntryCreateDesc newEntry = *entryReference;

            newEntry.inputPath = srcStr;
            newEntry.outputName = dstStr;
            validateCompressionLevel(newEntry.format, &newEntry.compressionLevel);
            newEntry.index = (*entryCount)++;

            arrpush(*entries, newEntry);
        }
    }

    fsDirectoryIteratorClose(iterator);
    return success;
}

static bool bunyArLibComputeEntryName(char*** strings, const struct BunyArLibEntryCreateDesc* entry, const char** outPath)
{
    const char* path = entry->outputName ? entry->outputName : entry->inputPath;

    size_t length = 0;
    if (fsIsNormalizedPath(path, '/'))
    {
        length = strlen(path);
        *outPath = path;
    }
    else
    {
        size_t pathLength = strlen(path);
        char*  str = (char*)tf_malloc(pathLength + 1);

        length = fsNormalizePath(path, '/', str);

        arrpush(*strings, str);
        *outPath = str;
    }

    if (strncmp(*outPath, "../", 3) == 0)
    {
        char buf[FS_MAX_PATH];
        fsAppendPathComponent(fsGetResourceDirectory(entry->inputRd), entry->inputPath, buf);

        LOGF(eERROR, "Entry name '%s' contains backlinks. Source path is '%s'", *outPath, buf);
        return false;
    }

    if (length > BUNYAR_FILE_NAME_LENGTH_MAX)
    {
        char buf[FS_MAX_PATH];
        fsAppendPathComponent(fsGetResourceDirectory(entry->inputRd), entry->inputPath, buf);

        LOGF(eERROR, "Entry name '%s' is longer than limit of %i. Source path is '%s'", *outPath, BUNYAR_FILE_NAME_LENGTH_MAX, buf);
        return false;
    }

    return true;
}

static int bunyArLibEntryDescCmp(const void* v0, const void* v1)
{
    const struct BunyArLibEntryCreateDesc* e0 = (const struct BunyArLibEntryCreateDesc*)v0;
    const struct BunyArLibEntryCreateDesc* e1 = (const struct BunyArLibEntryCreateDesc*)v1;
    return strcmp(e0->outputName, e1->outputName);
}

static bool bunyArLibCreatePreprocessDesc(const struct BunyArLibCreateDesc* inDesc, struct BunyArLibCreateDesc* outDesc, char*** strings)
{
    *outDesc = *inDesc;

    outDesc->entryCount = 0;
    outDesc->entries = NULL;

    if (inDesc->entryCount == 0)
        return true;

    bool success = true;
    for (uint64_t i = 0; i < inDesc->entryCount; ++i)
    {
        const struct BunyArLibEntryCreateDesc* entry = inDesc->entries + i;

        bool exist;
        bool isDir;
        bool isFile;
        if (!fsCheckPath(entry->inputRd, entry->inputPath, &exist, &isDir, &isFile))
        {
            success = false;
            break;
        }

        if (!exist)
        {
            if (entry->optional)
                continue;

            LOGF(eINFO, "Entry '%s' does not exist", entry->inputPath);
            success = false;
            break;
        }

        if (!isDir && !isFile)
        {
            LOGF(eINFO, "This entry can't be used for an archive: '%s'", entry->inputPath);
            success = false;
            break;
        }

        const char* outName = NULL;
        if (!bunyArLibComputeEntryName(strings, entry, &outName))
        {
            success = false;
            break;
        }

        if (isFile)
        {
            arrpush(outDesc->entries, *entry);

            size_t flen = arrlenu(outDesc->entries);

            struct BunyArLibEntryCreateDesc* newEntry = outDesc->entries + flen - 1;

            newEntry->outputName = outName;
            validateCompressionLevel(newEntry->format, &newEntry->compressionLevel);
            newEntry->index = outDesc->entryCount++;
            continue;
        }

        if (!bunyArLibCreateUnfoldDirectory(entry, &outDesc->entryCount, &outDesc->entries, strings, entry->inputPath, outName))
        {
            success = false;
            break;
        }
    }

    if (!success)
    {
        arrfree(outDesc->entries);
        memset(outDesc, 0, sizeof(*outDesc));
        return false;
    }

    ASSERT(outDesc->entryCount == arrlenu(outDesc->entries));

    if (outDesc->entryCount == 0)
        return true;

    qsort(outDesc->entries, outDesc->entryCount, sizeof(*outDesc->entries), bunyArLibEntryDescCmp);

    // Check and resolve duplicated entries
    // We consider collision as a settings override.
    // We return error if one output has two inputs.

    struct BunyArLibEntryCreateDesc* cur = outDesc->entries;
    struct BunyArLibEntryCreateDesc* end = outDesc->entries + outDesc->entryCount;

    for (uint64_t i = 0; i < outDesc->entryCount; ++i)
    {
        struct BunyArLibEntryCreateDesc* e0 = outDesc->entries + i;
        struct BunyArLibEntryCreateDesc* e1 = e0 + 1;
        if (e1 >= end || strcmp(e0->outputName, e1->outputName) != 0)
        {
            if (cur != e0)
                *cur = *e0;
            ++cur;
            continue;
        }

        // Check if input path is same.
        // We return error if one output has two inputs.
        if (e0->inputRd != e1->inputRd || strcmp(e0->inputPath, e1->inputPath) != 0)
        {
            const char* rd0 = fsGetResourceDirectory(e0->inputRd);
            const char* rd1 = fsGetResourceDirectory(e1->inputRd);

            const char* p0 = bunyArLibMergePath(strings, rd0, e0->inputPath, false);
            const char* p1 = bunyArLibMergePath(strings, rd1, e1->inputPath, false);

            fsNormalizePath(p0, '/', (char*)p0);
            fsNormalizePath(p1, '/', (char*)p1);

            if (strcmp(p0, p1) != 0)
            {
                LOGF(eERROR,
                     "Entry '%s' collision detected. "
                     "Source paths are dirrerent: '%s' vs '%s'",
                     e0->outputName, p0, p1);
                return false;
            }
        }

        // e0 is removed in the next loop.

        // Prefer later entry
        // This check is required because qsort has undefined behaviour:
        // "en.cppreference.com/w/cpp/algorithm/qsort"
        // >| If comp indicates two elements as equivalent, their order is
        // >| unspecified.
        if (e0->index > e1->index)
            *e1 = *e0; // Drop e1 instead of e0.
    }

    outDesc->entryCount = (uint64_t)(cur - outDesc->entries);

#if defined(FORGE_DEBUG)
    for (size_t i = 0; i < outDesc->entryCount; ++i)
    {
        struct BunyArLibEntryCreateDesc* entry = outDesc->entries + i;
        ASSERT(fsIsNormalizedPath(entry->outputName, '/'));
    }
#endif

    return true;
}

static void bunyArLibCreatePostprocessDesc(struct BunyArLibCreateDesc* desc, char*** strings)
{
    arrfree(desc->entries);
    desc->entries = NULL;

    for (uint64_t i = 0; i < (uint64_t)arrlenu(*strings); ++i)
        tf_free((*strings)[i]);
    arrfree(*strings);
    *strings = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibCreate (part two)                                     ///
/// Second step.                                                             ///
/// Prepare archive metadata                                                 ///
////////////////////////////////////////////////////////////////////////////////

struct BunyArLibCreateMetadata
{
    uint64_t                nodeCount;
    struct BunyArNode*      nodes;
    struct BunyArHashTable* hashTable;
    uint64_t                maxBlockSize;
    char*                   names;
    uint32_t                namesSize;
    bool                    lz4Used;
    bool                    zstdUsed;
};

// TODO experiment with this
static uint32_t convertBlockSize(enum BunyArFileFormat format, uint32_t blockSizeKb)
{
    switch (format)
    {
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
        if (blockSizeKb == BUNYAR_LIB_BLOCK_SIZE_KB_DEFAULT)
            return 256 * 1024;
        return blockSizeKb * 1024;
    default:
        return 256 * 1024;
    }
}

static void bunyArLibCreateMetadataDestroy(struct BunyArLibCreateMetadata* md)
{
    tf_free(md->nodes);
    tf_free(md->names);
    tf_free(md->hashTable);
    memset(md, 0, sizeof(*md));
}

static bool bunyArLibCreateMetadataDetail(const struct BunyArLibCreateDesc* desc, struct BunyArLibCreateMetadata* md)
{
    md->nodeCount = desc->entryCount;

    md->nodes = (struct BunyArNode*)tf_calloc(1, (sizeof(*md->nodes)) * md->nodeCount);
    if (!md->nodes)
        return false;

    md->maxBlockSize = 0;

    for (uint32_t ni = 0; ni < (uint32_t)desc->entryCount; ++ni)
    {
        struct BunyArNode*               node = md->nodes + ni;
        struct BunyArLibEntryCreateDesc* info = desc->entries + ni;

        node->namePointer.offset = md->namesSize;
        node->namePointer.size = (uint32_t)strlen(info->outputName);

        node->format = info->format;

        uint64_t blockSize = convertBlockSize(info->format, info->blockSizeKb);
        if (blockSize > md->maxBlockSize)
            md->maxBlockSize = blockSize;

        switch (node->format)
        {
        case BUNYAR_FILE_FORMAT_RAW:
            break;
        case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
            md->lz4Used = true;
            if (blockSize > LZ4_MAX_INPUT_SIZE)
            {
                LOGF(eERROR,
                     "Block size %lu is too large for LZ4 compression (limit is "
                     "%lu)",
                     (unsigned long)blockSize, (unsigned long)LZ4_MAX_INPUT_SIZE);
                return false;
            }
            break;
        case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
            md->zstdUsed = true;
            break;
        default:
            LOGF(eERROR, "Unsupported format: %llu\n", node->format);
            return false;
        }

        md->namesSize += node->namePointer.size + 1;
    }

    if (md->namesSize)
    {
        md->names = (char*)tf_malloc(md->namesSize);
        if (!md->names)
            return false;
    }
    else if (desc->entryCount)
    {
        LOGF(eERROR, "Empty entry names?");
        return false;
    }

    // initialize node names
    for (size_t fi = 0; fi < (size_t)desc->entryCount; ++fi)
    {
        struct BunyArNode* node = md->nodes + fi;
        memcpy(md->names + node->namePointer.offset, desc->entries[fi].outputName, node->namePointer.size + 1);
    }

    return true;
}

static bool bunyArLibCreateMetadata(const struct BunyArLibCreateDesc* desc, struct BunyArLibCreateMetadata* md)
{
    memset(md, 0, sizeof(*md));
    if (bunyArLibCreateMetadataDetail(desc, md))
        return true;
    bunyArLibCreateMetadataDestroy(md);
    return false;
}

static void hashTableTask(void* pUser)
{
    struct BunyArLibCreateMetadata* md = (struct BunyArLibCreateMetadata*)pUser;

    md->hashTable = bunyArHashTableConstruct(md->nodeCount, md->nodes, md->names);
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibCreate (part three)                                   ///
/// Third step.                                                              ///
/// Read input files. Compress them. Write archive.                          ///
///                                                                          ///
/// Multithreaded path:                                                      ///
/// Thread pool to read & compress files                                     ///
/// + thread to write archive through packet io                              ///
/// + thread to send tasks to thread pool and send packets to archive writer ///
///                                                                          ///
/// Singlethreaded path:                                                     ///
/// writer executes io receiver function which reads and compresses files    ///
////////////////////////////////////////////////////////////////////////////////

struct Packet
{
    struct FileAssemblyLine* file;
    struct FileBlock*        block;
};

struct PacketWaiterData
{
    uint64_t          nPacketsUsed;
    struct Packet*    packets;
    bool              kill;
    bool              done;
    struct Packet*    packetsQueue;
    Mutex             mutex;
    ConditionVariable condition;
};

enum BlockTaskStatusId
{
    BLOCK_TASK_STATUS_IDLE,
    BLOCK_TASK_STATUS_RUNNING,
    BLOCK_TASK_STATUS_COMPLETED,
    BLOCK_TASK_STATUS_ERROR,
};

struct FileBlock
{
    // variable data
    struct FileAssemblyLine* asmLine;

    uint64_t blockIndex;

    tfrg_atomic32_t compressStatusId_Atomic32;

    bool written;

    uint64_t rawSize;
    uint64_t compressedSize;

    // const data
    uint64_t bufferSize;
    uint8_t* bufferUncompressed;
    uint8_t* bufferCompressed;
};

typedef tfrg_atomicptr_t FileBlockAtomic;

struct FileAssemblyLine
{
    FileStream                             fileStream;
    struct ThreadsSharedMemory*            tsm;
    const struct BunyArLibEntryCreateDesc* entry;
    uint64_t                               entryIndex;
    uint64_t                               streamOffset;
    uint64_t                               fsize;
    uint64_t                               blockCount;
    uint64_t                               blockSize;
    FileBlockAtomic*                       blocks_AtomicPtr;
    uint64_t                               compressTaskBlockDone;
    uint64_t                               blockIndex;

    tfrg_atomic32_t readStatusId_Atomic32;

    bool error;

    bool writeComplete;
};

struct bunyArLibPacketIo
{
    bool (*receive)(void* pUser, struct FileAssemblyLine** outFile, struct FileBlock** outBlock);
    void* pUser;
};

struct ArchiveWriterThreadInfo
{
    ResourceDirectory                 rd;
    const char*                       dstPath;
    struct bunyArLibPacketIo          packetIo;
    const struct BunyArLibCreateDesc* desc;
    struct BunyArLibCreateMetadata*   md;
    bool*                             error;
};

struct ThreadsSharedMemory
{
    bool         singlethreadRun;
    uint64_t     nThreadItems;
    ThreadSystem threadSystem;

    bool error;

    struct CompressionContext* compressionContexts;

    tfrg_atomic64_t priorityEntryIndex_Atomic64;

    uint64_t                 maxAssemblyLines;
    struct FileAssemblyLine* assemblyLines;

    uint64_t          totalFileBlockCount;
    struct FileBlock* fileBlocks;

    void* globalAllocation;

    // unused blocks metadata
    uint64_t  nUnusedBlocks;
    uint64_t* unusedBlockIds;
    uint64_t  realBlockSize;

    // loks unused blocks metadata
    Mutex             mutexBlocks;
    // notify when nUnusedBlocks increments
    ConditionVariable conditionBlocks;

    struct PacketWaiterData packetIo;

    bool                           archiveError;
    struct ArchiveWriterThreadInfo writerInfo;
    ThreadHandle                   writerThread;

    bool              activity;
    Mutex             managerMutex;
    ConditionVariable managerCondition;

    ThreadHandle hashTableThread;
};

static void reportActivity(struct ThreadsSharedMemory* tsm)
{
    if (tsm->singlethreadRun)
        return;
    acquireMutex(&tsm->managerMutex);
    if (!tsm->activity)
        wakeOneConditionVariable(&tsm->managerCondition);
    tsm->activity = true;
    releaseMutex(&tsm->managerMutex);
}

struct CompressionContext
{
    void*      lz4Ctx;
    ZSTD_CCtx* zstdCtx;
};

static void resetBlock(struct FileBlock* block, struct FileAssemblyLine* file)
{
    ASSERT(tfrg_atomic32_load_relaxed(&block->compressStatusId_Atomic32) != BLOCK_TASK_STATUS_RUNNING);

    block->asmLine = file;
    block->blockIndex = 0;
    block->written = false;
    block->rawSize = 0;
    block->compressedSize = 0;
    tfrg_atomic32_store_relaxed(&block->compressStatusId_Atomic32, 0);
}

// If no free blocks available to acquire, it returns false
static bool getFileBlock(struct FileAssemblyLine* file, struct FileBlock** out)
{
    struct FileBlock* block = (struct FileBlock*)tfrg_atomicptr_load_relaxed(file->blocks_AtomicPtr + file->blockIndex);

    if (block)
    {
        *out = block;
        return true;
    }

    struct ThreadsSharedMemory* tsm = file->tsm;

    uint64_t pei = tfrg_atomic64_load_relaxed(&file->tsm->priorityEntryIndex_Atomic64);
    bool     priority = file->entryIndex == pei;

    uint64_t threshold = tsm->totalFileBlockCount / 2;
    if (priority)
        threshold = 0;

    acquireMutex(&tsm->mutexBlocks);

    if (tsm->nUnusedBlocks > threshold)
    {
        block = tsm->fileBlocks + tsm->unusedBlockIds[--tsm->nUnusedBlocks];
    }

    releaseMutex(&tsm->mutexBlocks);

    *out = block;

    if (block)
    {
        resetBlock(block, file);
        block->blockIndex = file->blockIndex;
        ASSERT(block->blockIndex + 1 <= file->blockCount);
        tfrg_atomicptr_store_relaxed(file->blocks_AtomicPtr + file->blockIndex, (uintptr_t)block);
        return true;
    }

    return false;
}

static void releaseFileBlock(struct FileAssemblyLine* file, struct FileBlock* block)
{
    if (!block)
    {
        file->writeComplete = true;
        reportActivity(file->tsm);
        return;
    }

    struct ThreadsSharedMemory* tsm = block->asmLine->tsm;

    tfrg_atomic64_store_relaxed(block->asmLine->blocks_AtomicPtr + block->blockIndex, 0);

    if (block->blockIndex + 1 == block->asmLine->blockCount)
    {
        block->asmLine->writeComplete = true;
    }

    resetBlock(block, NULL);

    uint64_t id = (uint64_t)(block - tsm->fileBlocks);
    acquireMutex(&tsm->mutexBlocks);
    tsm->unusedBlockIds[tsm->nUnusedBlocks++] = id;
    wakeOneConditionVariable(&tsm->conditionBlocks);
    releaseMutex(&tsm->mutexBlocks);

    reportActivity(tsm);
}

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

static bool compressionContextInit(struct CompressionContext* ctx, bool lz4FormatUsed, bool zstdFormatUsed)
{
    memset(ctx, 0, sizeof(*ctx));

    if (lz4FormatUsed)
    {
        int ctxSize = LZ4_sizeofState();
        int ctxSizeHc = LZ4_sizeofStateHC();
        ctx->lz4Ctx = tf_malloc((size_t)(ctxSize > ctxSizeHc ? ctxSize : ctxSizeHc));
        if (ctx->lz4Ctx == NULL)
            return false;
    }

    if (zstdFormatUsed)
    {
        ctx->zstdCtx = ZSTD_createCCtx_advanced(ZSTD_MEMORY_ALLOCATOR);
        if (ctx->zstdCtx == NULL)
            return false;
    }

    return true;
}

static void compressionContextDestroy(struct CompressionContext* ctx)
{
    tf_free(ctx->lz4Ctx);
    ZSTD_freeCCtx(ctx->zstdCtx);
    memset(ctx, 0, sizeof *ctx);
}

static bool bunyArLibTaskCompress(struct CompressionContext* ctx, enum BunyArFileFormat format, int compressionLevel, const void* src,
                                  uint64_t size, void* dst,
                                  uint64_t* dstLimitAndOutSize) // UINT64_MAX if not fit
{
    if (size == 0)
        return true;

    switch (format)
    {
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    {
        int compressedSize;

        if (compressionLevel <= 0)
        {
            int accelerationLevel = -compressionLevel;

            compressedSize = LZ4_compress_fast_extState(ctx->lz4Ctx, (const char*)src, (char*)dst, (int)size, (int)*dstLimitAndOutSize,
                                                        accelerationLevel);
        }
        else
        {
            compressedSize = LZ4_compress_HC_extStateHC(ctx->lz4Ctx, (const char*)src, (char*)dst, (int)size, (int)*dstLimitAndOutSize,
                                                        compressionLevel);
        }

        *dstLimitAndOutSize = compressedSize > 0 ? (uint64_t)compressedSize : UINT64_MAX;

        return true;
    }
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
    {
        size_t compressedSize = ZSTD_compressCCtx(ctx->zstdCtx, dst, *dstLimitAndOutSize, src, size, compressionLevel);

        ZSTD_ErrorCode error = ZSTD_getErrorCode(compressedSize);

        if (error == ZSTD_error_dstSize_tooSmall)
        {
            *dstLimitAndOutSize = UINT64_MAX;
            return true;
        }
        else if (error != ZSTD_error_no_error)
        {
            LOGF(eERROR, "ZSTD compression failure: %s\n", ZSTD_getErrorString(error));
            *dstLimitAndOutSize = 0;
            return false;
        }

        *dstLimitAndOutSize = compressedSize;
        return true;
    }
    default:
        ASSERT(false);
        exit(-1);
    }
}

static bool tf_seek(FileStream* fs, size_t pos) { return fsSeekStream(fs, SBO_START_OF_FILE, (ssize_t)pos); }

static bool tf_write(FileStream* fs, size_t size, void* data)
{
    size_t read = fsWriteToStream(fs, data, size);
    if (read != size)
    {
        LOGF(eERROR,
             "Failed to write %llu bytes to archive stream. %llu bytes written. "
             "Stream position is %lli",
             (unsigned long long)size, (unsigned long long)read, (long long)fsGetStreamSeekPosition(fs));
        return false;
    }
    return true;
}

static bool initializeEntryBlockStream(enum BunyArFileFormat format, uint32_t blockSizeKb, uint64_t fileSize,
                                       struct BunyArBlockFormatHeader* dstHeader, BunyArBlockPointer** dstBlocks,
                                       uint64_t* outBlocksWithHeaderSize)
{
    memset(dstHeader, 0, sizeof *dstHeader);
    *outBlocksWithHeaderSize = 0;

    switch (format)
    {
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
        break;
    default:
        return true;
    }

    uint64_t blockSize = convertBlockSize(format, blockSizeKb);
    uint64_t blockCount = fileSize / blockSize + ((fileSize % blockSize) > 0);
    size_t   blocksSize = sizeof(BunyArBlockPointer) * blockCount;

    if (blockCount)
    {
        void* mem = *dstBlocks;
        *dstBlocks = (BunyArBlockPointer*)tf_realloc(mem, blocksSize);
        if (!*dstBlocks)
        {
            tf_free(mem);
            return false;
        }
    }

    *outBlocksWithHeaderSize = sizeof(struct BunyArBlockFormatHeader) + blocksSize;

    dstHeader->blockSize = blockSize;
    dstHeader->blockSizeLast = fileSize - (fileSize / blockSize) * blockSize;
    if (dstHeader->blockSizeLast == 0)
        dstHeader->blockSizeLast = blockSize;
    dstHeader->blockCount = blockCount;
    return true;
}

// Writes archive file. Gets compressed file data through 'packetIo'.
// It just writes data given by 'packetIo' for each node one by one.
static bool bunyArLibArchiveWrite(ResourceDirectory rd, const char* dstPath, struct bunyArLibPacketIo packetIo,
                                  const struct BunyArLibCreateDesc* desc, struct BunyArLibCreateMetadata* md)
{
    enum BunyArLibWriteResult
    {
        BUNYAR_LIB_RESULT_SUCCESS,
        BUNYAR_LIB_RESULT_OUTPUT_ERROR,
        BUNYAR_LIB_RESULT_INPUT_ERROR,
        BUNYAR_LIB_RESULT_COMPRESSION_ERROR,
        BUNYAR_LIB_RESULT_MEMORY_ERROR,
    };

    FileStream archiveFs = { 0 };
    if (!fsOpenStreamFromPath(rd, dstPath, FM_WRITE, &archiveFs))
    {
        LOGF(eERROR, "Failed to create/open archive file '%s'", dstPath);
        return false;
    }

    enum BunyArLibWriteResult result = BUNYAR_LIB_RESULT_SUCCESS;

    uint64_t offset = sizeof(struct BunyArHeader) + desc->entryCount * sizeof(struct BunyArNode) + md->namesSize;

    uint64_t           filesDone = 0;
    struct BunyArNode* refNode = NULL;
    uint64_t           blockIndex = 0;

    uint64_t                       blockMetadataSize = 0;
    struct BunyArBlockFormatHeader blocksHeader = { 0 };
    BunyArBlockPointer*            blockPointers = NULL;

    size_t totalFilesSize = 0;

    int counterWidth = 0;

    if (desc->verbose)
    {
        char buffer[32];
        counterWidth = snprintf(buffer, sizeof buffer, "%llu", (unsigned long long)md->nodeCount);
    }

    int64_t time = getUSec(true);

    int prevPrintedLen = 0;

    struct FileAssemblyLine* file = NULL;
    struct FileBlock*        block = NULL;
    for (; packetIo.receive(packetIo.pUser, &file, &block); releaseFileBlock(file, block), ++blockIndex)
    {
        if (!file)
        {
            if (!block)
            {
                result = BUNYAR_LIB_RESULT_MEMORY_ERROR;
                break;
            }

            file = block->asmLine;
        }

        struct BunyArNode* node = md->nodes + file->entryIndex;
        if (refNode != node)
        {
            if (refNode || (block && blockIndex != block->blockIndex))
            {
                LOGF(eERROR, "Received unexpected file block");
                result = BUNYAR_LIB_RESULT_INPUT_ERROR;
                break;
            }

            node->filePointer.offset = offset;
            node->originalFileSize = file->fsize;

            totalFilesSize += file->fsize;

            if (!block)
                node->format = BUNYAR_FILE_FORMAT_RAW;

            prevPrintedLen = 0;

            if (desc->verbose)
            {
                prevPrintedLen += fprintf(stdout, "%*llu/%*llu ", counterWidth, (unsigned long long)file->entryIndex + 1, counterWidth,
                                          (unsigned long long)md->nodeCount);

                const char* name = md->names + node->namePointer.offset;

                prevPrintedLen += (int)utf8_strlen(name) + 1;
                fprintf(stdout, "'%s'", name);

                if (desc->verbose > 1)
                {
                    prevPrintedLen += fprintf(stdout, " %s", humanReadableSize(file->fsize).str);

                    if (node->format != BUNYAR_FILE_FORMAT_RAW)
                    {
                        prevPrintedLen += fprintf(stdout, " | %s %i\n", bunyArFormatName((enum BunyArFileFormat)node->format),
                                                  file->entry->compressionLevel);
                    }
                    else
                    {
                        prevPrintedLen += fprintf(stdout, " %s\n", bunyArFormatName(BUNYAR_FILE_FORMAT_RAW));
                    }
                }
                else
                {
                    putc('\n', stdout);
                    ++prevPrintedLen;
                }
            }

            if (!initializeEntryBlockStream((enum BunyArFileFormat)node->format, file->entry->blockSizeKb, file->fsize, &blocksHeader,
                                            &blockPointers, &blockMetadataSize))
            {
                result = BUNYAR_LIB_RESULT_MEMORY_ERROR;
                break;
            }

            offset += blockMetadataSize;

            node->filePointer.size += blockMetadataSize;

            if (!tf_seek(&archiveFs, offset))
            {
                result = BUNYAR_LIB_RESULT_OUTPUT_ERROR;
                break;
            }

            refNode = node;
        }

        size_t sizeToWrite = 0;
        void*  src = NULL;

        if (blocksHeader.blockSize)
        {
            if (!block || !blockPointers)
            {
                result = BUNYAR_LIB_RESULT_MEMORY_ERROR;
                break;
            }

            struct BunyArBlockInfo blockInfo = { 0 };

            blockInfo.isCompressed = block->rawSize > block->compressedSize;
            blockInfo.offset = node->filePointer.size - blockMetadataSize;
            blockInfo.size = blockInfo.isCompressed ? (uint32_t)block->compressedSize : (uint32_t)block->rawSize;

            if (!bunyArEncodeBlockPointer(blockInfo, blockPointers + block->blockIndex))
            {
                result = BUNYAR_LIB_RESULT_MEMORY_ERROR;
                break;
            }

            sizeToWrite = blockInfo.size;
            src = blockInfo.isCompressed ? block->bufferCompressed : block->bufferUncompressed;
        }
        else if (block)
        {
            sizeToWrite = block->rawSize;
            src = block->bufferUncompressed;
        }

        if (sizeToWrite)
        {
            node->filePointer.size += sizeToWrite;

            if (!tf_write(&archiveFs, sizeToWrite, src))
            {
                result = BUNYAR_LIB_RESULT_OUTPUT_ERROR;
                break;
            }

            if (desc->verbose)
            {
                int64_t newTime = getUSec(true);
                if (newTime - time > 1000 * 32)
                {
                    fprintf(stdout, " => %llu/%llu\r",
                            (unsigned long long)block->blockIndex + 1, //-V595 //-V522
                            (unsigned long long)file->blockCount);
                    fflush(stdout);
                    time = newTime;
                }
            }
        }

        // If file blocks streaming not done
        if (block && block->blockIndex + 1 < file->blockCount)
            continue;

        // Finish file

        ASSERT(node->format == BUNYAR_FILE_FORMAT_RAW || node->filePointer.size > 0);

        refNode = NULL;
        blockIndex = UINT64_MAX;

        offset = node->filePointer.offset + node->filePointer.size;

        if (blocksHeader.blockSize)
        {
            size_t headerSize = sizeof(struct BunyArBlockFormatHeader);
            size_t pointersSize = sizeof(BunyArBlockPointer) * blocksHeader.blockCount;

            // write blocks metadata at file start and go back
            if (!tf_seek(&archiveFs, node->filePointer.offset) || !tf_write(&archiveFs, headerSize, &blocksHeader) ||
                !tf_write(&archiveFs, pointersSize, blockPointers) || !tf_seek(&archiveFs, offset))
            {
                result = BUNYAR_LIB_RESULT_MEMORY_ERROR;
                break;
            }
        }

        ++filesDone;

        if (desc->verbose > 1)
        {
            if (node->format != BUNYAR_FILE_FORMAT_RAW)
            {
                fprintf(stdout, "\033[F");
                while (prevPrintedLen--)
                    fprintf(stdout, "\033[C");
                fprintf(stdout, "-> %s (x%.2f)\n", humanReadableSize(node->filePointer.size).str,
                        (double)node->originalFileSize / (double)node->filePointer.size);
            }
            else
                putc('\r', stdout);
        }

        if (desc->verbose > 2)
        {
            if (blocksHeader.blockCount)
            {
                fprintf(stdout, "|- ");
                bunyArLibPrintBlockAnalysis(&blocksHeader, blockPointers);
                putc('\n', stdout);
                putc('\n', stdout);
            }
            else
            {
                fputs("                   \n", stdout);
            }
        }
    }

    tf_free(blockPointers);

    size_t archiveSize = offset;

    if (result == BUNYAR_LIB_RESULT_SUCCESS && filesDone != md->nodeCount)
    {
        LOGF(eERROR, "%s", "Archive write can not continue, aborting.");
        result = BUNYAR_LIB_RESULT_INPUT_ERROR;
    }
    else if (result == BUNYAR_LIB_RESULT_SUCCESS)
    {
        size_t hashTableSize = bunyArHashTableSize(md->hashTable);

        if (desc->verbose > 1 && hashTableSize)
        {
            fprintf(stdout, "Hash Table %s\n\n", humanReadableSize(hashTableSize).str);
        }

        if (desc->verbose)
        {
            fprintf(stdout, "%s", "writing metadata ...\n");
        }

        struct BunyArHeader header = { 0 };
        memcpy(&header.magic, BUNYAR_MAGIC, sizeof(header.magic));

        header.nodesPointer.offset = sizeof(struct BunyArHeader);
        header.nodesPointer.size = sizeof(struct BunyArNode) * desc->entryCount;

        header.namesPointer.offset = header.nodesPointer.offset + header.nodesPointer.size;
        header.namesPointer.size = md->namesSize;

        header.hashTablePointer.offset = offset;
        header.hashTablePointer.size = hashTableSize;

        if (!tf_seek(&archiveFs, 0) || !tf_write(&archiveFs, sizeof(header), &header) ||
            !tf_write(&archiveFs, header.nodesPointer.size, md->nodes) || !tf_write(&archiveFs, header.namesPointer.size, md->names) ||
            (md->hashTable &&
             (!tf_seek(&archiveFs, header.hashTablePointer.offset) || !tf_write(&archiveFs, header.hashTablePointer.size, md->hashTable))))
            return BUNYAR_LIB_RESULT_OUTPUT_ERROR;

        if (desc->verbose > 1)
        {
            size_t metadataSize = sizeof(header) + header.nodesPointer.size + header.namesPointer.size + header.hashTablePointer.size;

            fprintf(stdout, "|- %s\n\n", humanReadableSize(metadataSize).str);
        }

        // because hash table is located in the end
        archiveSize = header.hashTablePointer.offset + header.hashTablePointer.size;
    }

    switch (result)
    {
    case BUNYAR_LIB_RESULT_SUCCESS:
        break;
    case BUNYAR_LIB_RESULT_OUTPUT_ERROR:
        LOGF(eERROR, "Output stream failure");
        break;
    case BUNYAR_LIB_RESULT_INPUT_ERROR:
        LOGF(eERROR, "Input stream failure");
        break;
    case BUNYAR_LIB_RESULT_MEMORY_ERROR:
        LOGF(eERROR, "Memory failure");
        break;
    case BUNYAR_LIB_RESULT_COMPRESSION_ERROR:
        LOGF(eERROR, "Compression failure");
        break;
    default:
        LOGF(eERROR, "Unknown failure");
        ASSERT(false);
    }

    if (!fsCloseStream(&archiveFs))
    {
        LOGF(eERROR, "Failed to close archive stream");
        result = BUNYAR_LIB_RESULT_OUTPUT_ERROR;
    }

    if (desc->verbose && result == BUNYAR_LIB_RESULT_SUCCESS)
    {
        fprintf(stdout, "Archive '%s' completed.\n|- %llu files\n|- %s -> %s (x%.2f)\n\n", dstPath, (unsigned long long)desc->entryCount,
                humanReadableSize(totalFilesSize).str, humanReadableSize(archiveSize).str, (double)totalFilesSize / (double)archiveSize);
    }

    return result == BUNYAR_LIB_RESULT_SUCCESS;
}

static void archiveWriterThreadFunc(void* pUser)
{
    struct ArchiveWriterThreadInfo* info = (struct ArchiveWriterThreadInfo*)pUser;

    *info->error = !bunyArLibArchiveWrite(info->rd, info->dstPath, info->packetIo, info->desc, info->md);
}

// this task won't loop file blocks because blocks compression executed parallel
static void fileAssemblyCompressTask(void* data, uint64_t thid)
{
    struct FileBlock* block = (struct FileBlock*)data;

    ASSERT(tfrg_atomic32_load_relaxed(&block->compressStatusId_Atomic32) == BLOCK_TASK_STATUS_RUNNING);

    struct FileAssemblyLine* file = block->asmLine;
    if (file->error)
        return;

    struct ThreadsSharedMemory* tsm = file->tsm;
    if (tsm->error)
        return;

    struct CompressionContext* ctx = tsm->compressionContexts + thid;

    block->compressedSize = block->bufferSize;
    if (!bunyArLibTaskCompress(ctx, file->entry->format, file->entry->compressionLevel, block->bufferUncompressed, block->rawSize,
                               block->bufferCompressed, &block->compressedSize))
    {
        tfrg_atomic32_store_relaxed(&block->compressStatusId_Atomic32, BLOCK_TASK_STATUS_ERROR);
        file->error = true;
        tsm->error = true;
    }
    else
    {
        tfrg_atomic32_store_relaxed(&block->compressStatusId_Atomic32, BLOCK_TASK_STATUS_COMPLETED);
    }

    reportActivity(tsm);
}

// this task loops file blocks because file reading is done in a single thread
static void fileAssemblyReadTask(void* data, uint64_t thid)
{
    struct FileAssemblyLine* file = (struct FileAssemblyLine*)data;
    if (file->error)
        goto ERROR_RETURN;
    ASSERT(tfrg_atomic32_load_relaxed(&file->readStatusId_Atomic32) == BLOCK_TASK_STATUS_RUNNING);

    if (!file->fileStream.pIO)
    {
        if (!fsOpenStreamFromPath(file->entry->inputRd, file->entry->inputPath, FM_READ | FM_ALLOW_READ, &file->fileStream))
        {
            char buffer[MAX_THREAD_NAME_LENGTH + 1];

            char path[FS_MAX_PATH];
            fsAppendPathComponent(fsGetResourceDirectory(file->entry->inputRd), file->entry->inputPath, path);

            LOGF(eERROR, "Failed to open file '%s' in thread %llu (%s)", path, (unsigned long long)thid,
                 (getCurrentThreadName(buffer, sizeof buffer), buffer));
            goto ERROR_RETURN;
        }

        file->streamOffset = 0;
        file->fsize = (size_t)fsGetStreamFileSize(&file->fileStream);

        if (file->entry->format == BUNYAR_FILE_FORMAT_RAW)
        {
            file->blockSize = file->tsm->realBlockSize;
        }
        else
        {
            file->blockSize = convertBlockSize(file->entry->format, file->entry->blockSizeKb);
        }

        file->blockCount = file->fsize / file->blockSize + ((file->fsize % file->blockSize) > 0);

        if (!file->blockCount)
            goto COMPLETE;

        ASSERT(!file->blocks_AtomicPtr);
        file->blocks_AtomicPtr = (FileBlockAtomic*)tf_calloc(1, sizeof *file->blocks_AtomicPtr * file->blockCount);
        if (!file->blocks_AtomicPtr)
            goto ERROR_RETURN;
    }

    for (;;)
    {
        struct FileBlock* block;
        if (!getFileBlock(file, &block))
        {
            // we have no available memory blocks
            // wait until compression & writer threads release blocks
            goto PAUSE;
        }

        block->rawSize = fsReadFromStream(&file->fileStream, block->bufferUncompressed, file->blockSize);

        file->streamOffset += block->rawSize;

        bool done = file->streamOffset >= file->fsize;

        if (!done && block->rawSize != file->blockSize)
        {
            LOGF(eERROR, "Unexpected end of file stream %s%s", fsGetResourceDirectory(file->entry->inputRd), file->entry->inputPath);
            goto ERROR_RETURN;
        }

        if (!done && block->rawSize == 0)
        {
            goto ERROR_RETURN;
        }

        struct ThreadsSharedMemory* tsm = file->tsm;
        if (tsm->error)
            goto ERROR_RETURN;

        if (file->entry->format == BUNYAR_FILE_FORMAT_RAW)
        {
            tfrg_atomic32_store_relaxed(&block->compressStatusId_Atomic32, BLOCK_TASK_STATUS_COMPLETED);
            reportActivity(tsm);
        }
        else
        {
            ASSERT(tfrg_atomic32_load_relaxed(&block->compressStatusId_Atomic32) != BLOCK_TASK_STATUS_COMPLETED);
            ASSERT((size_t)(block - tsm->fileBlocks) < tsm->totalFileBlockCount);

            tfrg_atomic32_store_relaxed(&block->compressStatusId_Atomic32, BLOCK_TASK_STATUS_RUNNING);

            threadSystemAddTask(tsm->threadSystem, fileAssemblyCompressTask, block);
        }

        if (done)
            goto COMPLETE;

        ++file->blockIndex;

        if (file->blockIndex >= file->blockCount)
        {
            LOGF(eERROR, "Unexpected end of file stream %s%s", fsGetResourceDirectory(file->entry->inputRd), file->entry->inputPath);
            goto ERROR_RETURN;
        }
    }

    if (0)
    {
    COMPLETE:
        fsCloseStream(&file->fileStream);
        tfrg_atomic32_store_relaxed(&file->readStatusId_Atomic32, BLOCK_TASK_STATUS_COMPLETED);
    }
    if (0)
    {
    PAUSE:
        tfrg_atomic32_store_relaxed(&file->readStatusId_Atomic32, BLOCK_TASK_STATUS_IDLE);
    }
    if (0)
    {
    ERROR_RETURN:
        fsCloseStream(&file->fileStream);
        file->error = true;
        file->tsm->error = true;
        tfrg_atomic32_store_relaxed(&file->readStatusId_Atomic32, BLOCK_TASK_STATUS_ERROR);
    }
    reportActivity(file->tsm);
    return;
}

static bool packetReceiverFunc(void* pUser, struct FileAssemblyLine** outFile, struct FileBlock** outBlock)
{
    struct PacketWaiterData* p = (struct PacketWaiterData*)pUser;

    for (; !p->kill;)
    {
        if (p->packets)
        {
            size_t nPackets = arrlenu(p->packets);
            if (p->nPacketsUsed < nPackets)
            {
                struct Packet packet;
                packet = p->packets[p->nPacketsUsed++];
                *outFile = packet.file;
                *outBlock = packet.block;
                return true;
            }

            arrfree(p->packets);
            p->packets = NULL;
            p->nPacketsUsed = 0;
        }

        acquireMutex(&p->mutex);
        while (p->packetsQueue == NULL && !p->done)
        {
            waitConditionVariable(&p->condition, &p->mutex, TIMEOUT_INFINITE);
        }
        p->packets = p->packetsQueue;
        p->packetsQueue = NULL;
        releaseMutex(&p->mutex);

        if (p->packets == NULL && p->done)
            return false;
    }

    return false;
}

static void packetSend(struct PacketWaiterData* p, struct FileAssemblyLine* file, struct FileBlock* block)
{
    acquireMutex(&p->mutex);
    struct Packet packet = { file, block };
    arrpush(p->packetsQueue, packet);
    wakeOneConditionVariable(&p->condition);
    releaseMutex(&p->mutex);
}

static void destroyAssemblyLine(struct FileAssemblyLine* file)
{
    fsCloseStream(&file->fileStream);
    tf_free((void*)file->blocks_AtomicPtr);
    memset(file, 0, sizeof *file);
}

// send requests to read files. Read file task sends task to compress or write
// compressed task puts ready data to file bucket
// this loop pushes packets in the correct order to archive
static void bunyArLibCreateTaskManager(struct ThreadsSharedMemory* tsm, const struct BunyArLibCreateDesc* desc)
{
    uint64_t lastExecutedEntry = 0;

    uint64_t blockId = 0;
    uint64_t entryId = 0;

    while (!tsm->error && !tsm->archiveError && entryId < desc->entryCount)
    {
        bool somethingHappened = false;

        for (uint64_t i = 0; i < tsm->maxAssemblyLines; ++i)
        {
            struct FileAssemblyLine* file = tsm->assemblyLines + i;

            if (!file->entry)
            {
                if (lastExecutedEntry >= desc->entryCount)
                    continue;

                const struct BunyArLibEntryCreateDesc* entry = desc->entries + lastExecutedEntry;

                file->tsm = tsm;
                file->entry = entry;
                file->entryIndex = lastExecutedEntry;

                ++lastExecutedEntry;

                somethingHappened = true;
                continue;
            }

            enum BlockTaskStatusId readStatus = (enum BlockTaskStatusId)tfrg_atomic32_load_relaxed(&file->readStatusId_Atomic32);

            if (readStatus == BLOCK_TASK_STATUS_IDLE)
            {
                if (file->error)
                {
                    tsm->error = true;
                    break;
                }

                tfrg_atomic32_store_relaxed(&file->readStatusId_Atomic32, BLOCK_TASK_STATUS_RUNNING);

                threadSystemAddTask(tsm->threadSystem, fileAssemblyReadTask, file);

                somethingHappened = true;
            }
            else if (readStatus == BLOCK_TASK_STATUS_COMPLETED && file->writeComplete)
            {
                destroyAssemblyLine(file);
                somethingHappened = true;
                continue;
            }

            if (file->entryIndex != entryId)
                continue;

            if (readStatus == BLOCK_TASK_STATUS_COMPLETED && file->fsize == 0)
            {
                packetSend(&tsm->packetIo, file, NULL);
            NEXT_ENTRY:
                ++entryId;
                blockId = 0;
                tfrg_atomic64_store_relaxed(&tsm->priorityEntryIndex_Atomic64, entryId);
                somethingHappened = true;
                continue;
            }

            if (!file->blocks_AtomicPtr)
                continue;

            struct FileBlock* block = (struct FileBlock*)tfrg_atomicptr_load_relaxed(file->blocks_AtomicPtr + blockId);
            if (!block)
                continue;

            if (tfrg_atomic32_load_relaxed(&block->compressStatusId_Atomic32) == BLOCK_TASK_STATUS_COMPLETED)
            {
                packetSend(&tsm->packetIo, NULL, block);
                ++blockId;

                somethingHappened = true;
            }

            if (blockId == file->blockCount)
                goto NEXT_ENTRY;
        }

        if (somethingHappened)
            continue;

        acquireMutex(&tsm->managerMutex);
        while (!tsm->activity)
        {
            waitConditionVariable(&tsm->managerCondition, &tsm->managerMutex, TIMEOUT_INFINITE);
        }
        tsm->activity = false;
        releaseMutex(&tsm->managerMutex);
    }
}

static void destroyThreadSharedMemory(struct ThreadsSharedMemory* tsm)
{
    threadSystemExit(&tsm->threadSystem, &gThreadSystemExitDescDefault);

    if (tsm->hashTableThread)
        joinThread(tsm->hashTableThread);

    if (tsm->compressionContexts)
    {
        for (uint64_t i = 0; i < tsm->nThreadItems; ++i)
            compressionContextDestroy(tsm->compressionContexts + i);
        tf_free(tsm->compressionContexts);
    }

    for (uint64_t i = 0; i < tsm->maxAssemblyLines; ++i)
        destroyAssemblyLine(tsm->assemblyLines + i);

    tf_free(tsm->assemblyLines);

    tf_free(tsm->fileBlocks);

    tf_free(tsm->globalAllocation);
    tf_free(tsm->unusedBlockIds);

    destroyMutex(&tsm->mutexBlocks);
    destroyConditionVariable(&tsm->conditionBlocks);

    arrfree(tsm->packetIo.packetsQueue);
    arrfree(tsm->packetIo.packets);
    destroyMutex(&tsm->packetIo.mutex);
    destroyConditionVariable(&tsm->packetIo.condition);

    destroyMutex(&tsm->managerMutex);
    destroyConditionVariable(&tsm->managerCondition);

    memset(tsm, 0, sizeof(*tsm));
}

static bool initThreadSharedMemory(struct ThreadsSharedMemory* tsm, ResourceDirectory rd, const char* dstPath,
                                   const struct BunyArLibCreateDesc* desc, struct BunyArLibCreateMetadata* md)
{
    memset(tsm, 0, sizeof(*tsm));

    // waiter + scheduler + thread pool
    uint64_t threadPoolSize = (uint64_t)desc->threadPoolSize;

    if (desc->threadPoolSize < 0)
        threadPoolSize = getNumCPUCores();

    struct ThreadSystemInitDesc tsInfo = { 0 };

    tsInfo.threadCount = threadPoolSize;

    if (!threadSystemInit(&tsm->threadSystem, &tsInfo))
    {
        LOGF(eERROR, "Failed to start thread pool");
    ERROR_RETURN:
        destroyThreadSharedMemory(tsm);
        return false;
    }

    tsm->nThreadItems = threadPoolSize;
    // Init for single-threaded use
    if (tsm->nThreadItems == 0)
        tsm->nThreadItems = 1;

    if (md->lz4Used || md->zstdUsed)
    {
        tsm->compressionContexts = (struct CompressionContext*)tf_calloc(1, sizeof *tsm->compressionContexts * tsm->nThreadItems);
        if (!tsm->compressionContexts)
            goto ERROR_RETURN;

        for (uint32_t i = 0; i < tsm->nThreadItems; ++i)
        {
            if (!compressionContextInit(tsm->compressionContexts + i, md->lz4Used, md->zstdUsed))
                goto ERROR_RETURN;
        }
    }

    tsm->maxAssemblyLines = desc->maxParallelFileReads == 0 ? 4 : desc->maxParallelFileReads;

    if (desc->entryCount < tsm->maxAssemblyLines)
        tsm->maxAssemblyLines = desc->entryCount;
    if (threadPoolSize < tsm->maxAssemblyLines)
        tsm->maxAssemblyLines = threadPoolSize;
    if (tsm->maxAssemblyLines < 1)
        tsm->maxAssemblyLines = 1;

    tsm->assemblyLines = (struct FileAssemblyLine*)tf_calloc(1, sizeof *tsm->assemblyLines * tsm->maxAssemblyLines);
    if (!tsm->assemblyLines)
        goto ERROR_RETURN;

    if (!initMutex(&tsm->mutexBlocks))
        goto ERROR_RETURN;
    if (!initConditionVariable(&tsm->conditionBlocks))
        goto ERROR_RETURN;

    uint64_t threadMemorySize = desc->memorySizePerThread;
    if (threadMemorySize == 0)
        threadMemorySize = 1024 * 1024 * 4;
    else if (threadMemorySize < 1024 * 256)
        threadMemorySize = 1024 * 256;

    uint64_t memorySize = tsm->nThreadItems * threadMemorySize;

    uint64_t blockSize = md->maxBlockSize;
    uint64_t nBlocks = blockSize ? memorySize / (blockSize * 2) : 0;

    tsm->totalFileBlockCount = nBlocks;

    tsm->nUnusedBlocks = nBlocks;
    tsm->unusedBlockIds = (uint64_t*)tf_malloc(sizeof *tsm->unusedBlockIds * nBlocks);
    if (!tsm->unusedBlockIds)
        goto ERROR_RETURN;

    for (uint64_t i = 0; i < nBlocks; ++i)
        tsm->unusedBlockIds[i] = i;

    tsm->globalAllocation = tf_malloc(memorySize);
    if (!tsm->globalAllocation)
        goto ERROR_RETURN;

    tsm->fileBlocks = (struct FileBlock*)tf_calloc(1, sizeof *tsm->fileBlocks * nBlocks);
    if (!tsm->fileBlocks)
        goto ERROR_RETURN;

    uint8_t* mem = (uint8_t*)tsm->globalAllocation;
    for (uint64_t i = 0; i < nBlocks; ++i)
    {
        struct FileBlock* block = tsm->fileBlocks + i;

        block->bufferSize = blockSize;

        block->bufferUncompressed = mem;
        mem += blockSize;
        block->bufferCompressed = mem;
        mem += blockSize;
    }

    tsm->realBlockSize = blockSize * 2;

    if (!desc->skipHashTable)
    {
        ThreadDesc threadInfo = { 0 };
        threadInfo.pFunc = hashTableTask;
        threadInfo.pData = md;
        sprintf(threadInfo.mThreadName, "HashTable");
        if (!initThread(&threadInfo, &tsm->hashTableThread))
            goto ERROR_RETURN;
    }

    tsm->singlethreadRun = threadPoolSize == 0;
    if (tsm->singlethreadRun)
        return true;

    if (!initConditionVariable(&tsm->packetIo.condition))
        goto ERROR_RETURN;

    if (!initMutex(&tsm->packetIo.mutex))
        goto ERROR_RETURN;

    if (!initConditionVariable(&tsm->managerCondition))
        goto ERROR_RETURN;

    if (!initMutex(&tsm->managerMutex))
        goto ERROR_RETURN;

    tsm->writerInfo.rd = rd;
    tsm->writerInfo.dstPath = dstPath;
    tsm->writerInfo.desc = desc;
    tsm->writerInfo.md = md;
    tsm->writerInfo.error = &tsm->archiveError;

    tsm->writerInfo.packetIo.receive = packetReceiverFunc;
    tsm->writerInfo.packetIo.pUser = &tsm->packetIo;

    struct ThreadDesc threadInfo = { 0 };

    threadInfo.pFunc = archiveWriterThreadFunc;
    threadInfo.pData = &tsm->writerInfo;
    snprintf(threadInfo.mThreadName, sizeof threadInfo.mThreadName, "ArchiveWriter");

    if (!initThread(&threadInfo, &tsm->writerThread))
        goto ERROR_RETURN;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibCreate (singlethreaded)                               ///
////////////////////////////////////////////////////////////////////////////////

struct bunyArLibCreateSingleThreadContext
{
    struct ThreadsSharedMemory*       tsm;
    const struct BunyArLibCreateDesc* desc;

    uint64_t blockId;
    uint64_t entryId;
};

// _Atomic variable are used directly, because we have only one thread
static bool packetSingleThreadProducerFunc(void* pUser, struct FileAssemblyLine** outFile, struct FileBlock** outBlock)
{
    *outFile = NULL;
    *outBlock = NULL;

    struct bunyArLibCreateSingleThreadContext* ctx = (struct bunyArLibCreateSingleThreadContext*)pUser;

    struct ThreadsSharedMemory*       tsm = ctx->tsm;
    const struct BunyArLibCreateDesc* desc = ctx->desc;

    if (tsm->error || tsm->archiveError)
        return false;
    if (ctx->entryId >= desc->entryCount)
        return false;

    struct FileAssemblyLine* file = tsm->assemblyLines;
    if (file->error)
        return false;

    if (file->writeComplete)
    {
        destroyAssemblyLine(file);
        ++ctx->entryId;
        ctx->blockId = 0;

        tsm->priorityEntryIndex_Atomic64 = ctx->entryId;
        if (ctx->entryId >= desc->entryCount)
        {
            if (tsm->hashTableThread)
            {
                joinThread(tsm->hashTableThread);
                memset(&tsm->hashTableThread, 0, sizeof tsm->hashTableThread);
            }
            return false;
        }
    }

    if (!file->entry)
    {
        const struct BunyArLibEntryCreateDesc* entry = desc->entries + ctx->entryId;

        file->tsm = tsm;
        file->entry = entry;
        file->entryIndex = ctx->entryId;
    }

    for (; !tsm->error;)
    {
        if (file->readStatusId_Atomic32 == BLOCK_TASK_STATUS_IDLE) // atomic
                                                                   // read
        {
            file->readStatusId_Atomic32 = BLOCK_TASK_STATUS_RUNNING; // atomic write
            fileAssemblyReadTask(file, 0);
        }

        if (file->error)
        {
            tsm->error = true;
            return false;
        }

        // atomic read
        if (file->readStatusId_Atomic32 == BLOCK_TASK_STATUS_COMPLETED && !file->blocks_AtomicPtr)
        {
            *outFile = file;
            return true;
        }

        if (!file->blocks_AtomicPtr)
            continue;

        // packet receiver must mark file->writeComplete or exit
        ASSERT(ctx->blockId < file->blockCount);

        struct FileBlock* block = (struct FileBlock*)file->blocks_AtomicPtr[ctx->blockId]; // atomic read
        if (!block)
            continue;

        // atomic read
        if (block->compressStatusId_Atomic32 == BLOCK_TASK_STATUS_COMPLETED)
        {
            ++ctx->blockId;
            *outBlock = block;
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibCreate (glue part)                                    ///
////////////////////////////////////////////////////////////////////////////////

// from lz4.c
static const int LZ4_ACCELERATION_MAX = 65537;

void bunyArLibCompressionLevelLimits(enum BunyArFileFormat format, int* min, int* max)
{
    switch (format)
    {
    case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
        *min = -LZ4_ACCELERATION_MAX;
        *max = LZ4HC_CLEVEL_MAX;
        break;
    case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
        *min = ZSTD_minCLevel();
        *max = ZSTD_maxCLevel();
        break;
    case BUNYAR_FILE_FORMAT_RAW:
    default:
        *min = 0;
        *max = 0;
        break;
    }
}

static bool bunyArLibCreateArchive(ResourceDirectory rd, const char* dstPath, const struct BunyArLibCreateDesc* desc,
                                   struct BunyArLibCreateMetadata* md)
{
    bool singleThreaded = desc->threadPoolSize == 0;

    struct ThreadsSharedMemory tsm;
    if (!initThreadSharedMemory(&tsm, rd, dstPath, desc, md))
    {
        LOGF(eERROR, "Failed to initialize thread infrastructure");
        return false;
    }

    if (singleThreaded)
    {
        struct bunyArLibCreateSingleThreadContext ctx = { 0 };

        ctx.tsm = &tsm;
        ctx.desc = desc;

        struct bunyArLibPacketIo io = { packetSingleThreadProducerFunc, &ctx };

        tsm.archiveError = !bunyArLibArchiveWrite(rd, dstPath, io, desc, md);
    }
    else
    {
        bunyArLibCreateTaskManager(&tsm, desc);
        if (tsm.hashTableThread)
        {
            joinThread(tsm.hashTableThread);
            memset(&tsm.hashTableThread, 0, sizeof tsm.hashTableThread);
        }

        if (tsm.error)
            tsm.packetIo.kill = true;
        tsm.packetIo.done = true;

        wakeOneConditionVariable(&tsm.packetIo.condition);
        joinThread(tsm.writerThread);
    }

    bool success = !tsm.archiveError && !tsm.error;

    destroyThreadSharedMemory(&tsm);
    return success;
}

bool bunyArLibCreate(ResourceDirectory rd, const char* dstPath, const struct BunyArLibCreateDesc* inDesc)
{
    char** strings = NULL;

    struct BunyArLibCreateDesc desc;
    if (!bunyArLibCreatePreprocessDesc(inDesc, &desc, &strings))
    {
        LOGF(eERROR, "Failed to preprocess file entries to create archive '%s'", dstPath);
        bunyArLibCreatePostprocessDesc(&desc, &strings);
        return false;
    }

    struct BunyArLibCreateMetadata md;
    bool                           success = bunyArLibCreateMetadata(&desc, &md);

    if (!success)
    {
        LOGF(eERROR, "Failed to initialize metadata for archive '%s'", dstPath);
    }

    if (success)
        success = bunyArLibCreateArchive(rd, dstPath, &desc, &md);

    bunyArLibCreateMetadataDestroy(&md);
    bunyArLibCreatePostprocessDesc(&desc, &strings);
    return success;
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibExtract                                               ///
////////////////////////////////////////////////////////////////////////////////

bool bunyArLibExtract(struct IFileSystem* archive, ResourceDirectory rd, const char* dstDir, const struct BunyArLibExtractDesc* desc)
{
    struct BunyArDescription archiveInfo;
    fsArchiveGetDescription(archive, &archiveInfo);

    FileStream fsOut = { 0 };
    FileStream fsIn = { 0 };

    bool loopFileNames = desc->fileNameCount > 0;

    uint64_t loopLimit = loopFileNames ? desc->fileNameCount : archiveInfo.nodeCount;

    int counterWidth = 0;
    int printedLength = 0;
    if (desc->verbose)
    {
        char buffer[32];
        counterWidth = snprintf(buffer, sizeof buffer, "%llu", (unsigned long long)loopLimit);
    }

    uint64_t failureCount = 0;

    for (uint64_t i = 0; i < loopLimit; ++i)
    {
        const char* error = NULL;

        uint64_t uid = i;

        uint64_t totalSize = 0;
        char     buffer[64 * 1024];
        ASSERT(sizeof buffer >= FS_MAX_PATH);

        struct BunyArNodeDescription node;

        if (loopFileNames && !fsArchiveGetNodeId(archive, desc->fileNames[i], &uid))
        {
            node.name = desc->fileNames[i];
            error = "no such file";
            goto FILE_DONE;
        }

        fsArchiveGetNodeDescription(archive, uid, &node);

        if (desc->verbose)
        {
            printedLength =
                fprintf(stdout, "%*llu/%*llu ", counterWidth, (unsigned long long)i + 1, counterWidth, (unsigned long long)loopLimit);

            fprintf(stdout, "'%s'\n", node.name);
        }

        if (strncmp(node.name, "../", 3) == 0)
        {
            LOGF(eERROR, "Archive file '%s' contains backlinks, so it is skipped.", node.name);
            continue;
        }

        if (desc->verbose > 1)
        {
            printedLength += (int)utf8_strlen(node.name) + 1;

            fprintf(stdout, "\033[F");
            while (printedLength--)
                fprintf(stdout, "\033[C");
            fprintf(stdout, " %s %s -> %s (x%.2f)\n", bunyArFormatName((enum BunyArFileFormat)node.format),
                    humanReadableSize(node.compressedSize).str, humanReadableSize(node.fileSize).str,
                    (double)node.fileSize / (double)node.compressedSize);
        }

        if (!fsIoOpenByUid(archive, uid, FM_READ, &fsIn))
        {
            error = "file is corrupted or archive stream failure";
            goto FILE_DONE;
        }

        { // prepare directory
            char subDir[FS_MAX_PATH];
            fsGetParentPath(node.name, subDir);
            fsAppendPathComponent(dstDir, subDir, buffer);
            if (!fsCreateDirectory(rd, buffer, true))
            {
                error = "failed to create directory";
                goto FILE_DONE;
            }
        }

        fsAppendPathComponent(dstDir, node.name, buffer);
        if (!fsOpenStreamFromPath(rd, buffer, FM_WRITE, &fsOut))
        {
            error = "failed to create output file";
            goto FILE_DONE;
        }

        while (!fsStreamAtEnd(&fsIn))
        {
            size_t size = fsReadFromStream(&fsIn, buffer, sizeof(buffer));
            if (size == 0)
                break;

            if (fsWriteToStream(&fsOut, buffer, size) != size)
            {
                error = "failed to write data to output file";
                goto FILE_DONE;
            }

            totalSize += size;
        }

        if (totalSize != node.fileSize)
        {
            error = "File size mismatch";
            goto FILE_DONE;
        }

    FILE_DONE:
        fsCloseStream(&fsIn);
        fsCloseStream(&fsOut);

        if (error)
        {
            ++failureCount;
            LOGF(eERROR, "Failed to extract file '%s': %s", node.name, error);
            if (desc->continueOnError)
            {
                putc('\n', stdout);
                continue;
            }
            return false;
        }
    }

    if (desc->verbose && failureCount)
    {
        fprintf(stdout, "\nSome files are not extracted (%llu/%llu)\n", (unsigned long long)loopLimit - failureCount,
                (unsigned long long)loopLimit);
    }
    else if (desc->verbose)
    {
        fprintf(stdout, "\nAll files are extracted (%llu)\n", (unsigned long long)loopLimit);
    }

    return failureCount == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibPrintBlockAnalysis                                    ///
////////////////////////////////////////////////////////////////////////////////

void bunyArLibPrintBlockAnalysis(const struct BunyArBlockFormatHeader* header, const BunyArBlockPointer* blocks)
{
    uint64_t nRaws = 0;
    uint64_t sizeCompressed = 0;
    uint64_t rawpercent = 0;
    double   avgCompression = 1;

    if (header->blockCount)
    {
        for (uint64_t bi = 0; bi < header->blockCount - 1; ++bi)
        {
            struct BunyArBlockInfo block = bunyArDecodeBlockPointer(blocks[bi]);

            if (block.isCompressed)
                sizeCompressed += block.size;
            else
                ++nRaws;
        }

        struct BunyArBlockInfo lastblock = bunyArDecodeBlockPointer(blocks[header->blockCount - 1]);

        if (lastblock.isCompressed)
            sizeCompressed += lastblock.size;

        rawpercent = ((nRaws + !lastblock.isCompressed) * 100) / header->blockCount;

        if (sizeCompressed)
        {
            uint64_t uncompressedSize = ((header->blockCount - 1) - nRaws) * header->blockSize;

            if (lastblock.isCompressed)
                uncompressedSize += header->blockSizeLast;

            avgCompression = (double)uncompressedSize / (double)sizeCompressed;
        }
    }

    fprintf(stdout, "%llu of %lluKB blocks, %llu%% raw", (unsigned long long)header->blockCount,
            (unsigned long long)header->blockSize / 1024, (unsigned long long)rawpercent);

    if (rawpercent != 0 && rawpercent != 100)
        fprintf(stdout, " (%.2f non-raw rate)", avgCompression);
}

////////////////////////////////////////////////////////////////////////////////
/// Function bunyArLibHashTableBenchmarks                                   ///
////////////////////////////////////////////////////////////////////////////////

static int BunyArNodeNodeCmp(const void* v0, const void* v1)
{
    const struct BunyArNode* n0 = (const struct BunyArNode*)v0;
    const struct BunyArNode* n1 = (const struct BunyArNode*)v1;

    const char* names = NULL;
    memcpy((void*)&names, (const void*)&n0->filePointer, sizeof(char*)); //-V512_UNDERFLOW_OFF

    return strcmp(names + n0->namePointer.offset, names + n1->namePointer.offset);
}

static void randomizeStr(char* str, size_t length)
{
    const char CHARSET[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/.-_0123456789";

    if (!length)
        return;

    for (size_t n = 0; n < length - 1; n++)
        str[n] = CHARSET[rand() % (int)(sizeof CHARSET - 1)];
    str[length - 1] = 0;
}

bool bunyArLibHashTableBenchmarks(size_t keyCount, size_t keySize)
{
    if (keyCount == 0 || keySize == 0)
        return true;

    if (keySize < 8 * 4)
    {
        // TODO This is just approximation
        size_t klimit = (size_t)1 << (((keySize * 8) * 6) / 8);
        if (klimit < keyCount)
        {
            LOGF(eERROR,
                 "Key size %lluB (%llub) is too small to support %llu unique "
                 "keys.",
                 (unsigned long long)keySize, (unsigned long long)keySize * 8, (unsigned long long)keyCount);
            return false;
        }
    }

    bool success = true;

    //////////////////////////////////////
    // Generate unique keys (slowest part)

    size_t keyStrSize = keySize + 1;

    size_t allSize = (keyStrSize + sizeof(struct BunyArNode)) * keyCount;

    LOGF(eINFO, "Allocating %s memory for archive nodes and keys.", humanReadableSize(allSize).str);

    struct BunyArNode* nodes = (struct BunyArNode*)tf_calloc(1, allSize);
    char*              names = (char*)(nodes + keyCount);

    int64_t startTime = getUSec(true);

    for (size_t i = 0; i < keyCount; ++i)
    {
        struct BunyArPointer32 strptr = {
            (uint32_t)(keyStrSize * i),
            (uint32_t)(keySize),
        };
        nodes[i].namePointer = strptr;
        randomizeStr(names + strptr.offset, strptr.size + 1);

        memcpy((void*)&nodes[i].filePointer, &names, sizeof(char*)); //-V512_UNDERFLOW_OFF
    }

    // Get rid of random keys duplicates. This part is very expensive while it
    // is not the part of hash table construction
    for (;;)
    {
        qsort(nodes, keyCount, sizeof(*nodes), BunyArNodeNodeCmp);

        bool duplicate = false;
        for (size_t i = 0; i < keyCount - 1; ++i)
        {
            if (strcmp(names + nodes[i].namePointer.offset, names + nodes[i + 1].namePointer.offset) == 0)
            {
                duplicate = true;
                randomizeStr(names + nodes[i].namePointer.offset, keyStrSize);
            }
        }

        if (!duplicate)
            break;
    }

    int64_t endTime = getUSec(true);

    LOGF(eINFO, "%llu unique %llu-bit keys generated in %s", (unsigned long long)keyCount, (unsigned long long)keySize * 8,
         humanReadableTime(endTime - startTime).str);

    ///////////////////
    // Contruction test

    startTime = getUSec(true);

    struct BunyArHashTable* hashTable = bunyArHashTableConstruct(keyCount, nodes, names);

    endTime = getUSec(true);

    size_t htsize = bunyArHashTableSize(hashTable);

    LOGF(eINFO, "Archive hash table %s for %llu keys in %s. %s (%f bits/key).", hashTable ? "construction" : "construction failure",
         (unsigned long long)keyCount, humanReadableTime(endTime - startTime).str, humanReadableSize(htsize).str,
         (double)(8 * htsize) / (double)keyCount);

    if (!hashTable)
    {
        success = false;
        goto CLEANUP;
    }

    //////////////
    // Lookup test

    startTime = getUSec(true);

    for (size_t i = 0; i < keyCount; ++i)
    {
        uint64_t value = bunyArHashTableLookup(hashTable, names + nodes[i].namePointer.offset, keyCount, nodes, names);

        if (value >= keyCount)
        {
            LOGF(eERROR, "Archive hash table lookup test failed: key wasn't found.");
            success = false;
            goto CLEANUP;
        }

        if (value != i)
        {
            LOGF(eERROR, "Archive hash table lookup test failed: got the wrong key.");
            success = false;
            goto CLEANUP;
        }
    }

    endTime = getUSec(true);

    LOGF(eINFO, "Archive hash table lookup for %llu keys in %s. %s/key", (unsigned long long)keyCount,
         humanReadableTime(endTime - startTime).str, humanReadableTimeD((double)(endTime - startTime) / (double)keyCount).str);

    ///////////////////////
    // Cleanup

CLEANUP:
    tf_free(hashTable);
    tf_free(nodes);

    return success;
}
