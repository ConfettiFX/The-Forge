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

#include <locale.h>

#include "../../Utilities/Interfaces/ILog.h"

#include "Buny.h"

#if defined(_WINDOWS)
static const char DIR_SEP = '\\';
#else
static const char DIR_SEP = '/';
#endif

static const char BUNYAR_TOOL_NAME[] = "buny";

static const enum ResourceDirectory TF_RD = 0;

enum ArgType
{
    AT_UNRECOGNIZED,
    AT_VERBOSITY,
    AT_FORMAT,
    AT_COMPRESSION_LEVEL,
    AT_BLOCK_SIZE,
    AT_HASHMAP,
    AT_OPTIONAL,
    AT_BLOCKS,
    AT_NAME,
    AT_HELP,
    AT_CONTINUE_ON_ERROR,
    AT_KEY_COUNT,
    AT_KEYSIZE,
    AT_PARALLEL_READS,
    AT_MEMORY_SIZE,
    AT_THREADS,
};

struct ArgTracker
{
    const char*  name;
    enum ArgType argType;
    long         min;
    long         max;
    const char*  help;
};

struct BunyArToolCtx
{
    // archive create flags
    bool hashMap;

    // archive create entry args
    size_t                outputNameCutLength; // only set by drag&drop
    enum BunyArFileFormat format;
    uint32_t              blockSizeKb;
    int                   zstdcl;
    int                   lz4cl;
    bool                  optional;
    char*                 outputName;
    int                   threadCount;
    size_t                parallelFileReads;
    size_t                MBPerThread;

    // inspect
    bool inspectBlocks;

    // extract
    bool keepGoing;

    // benchmark
    size_t keyCount;
    size_t keySize;

    // global
    bool     archivePathDontWanna;
    char*    archivePath;
    unsigned verbose;
    bool     help;

    // lazy shortcut
    size_t argCount;

    // iterator
    char** argCur;
    char** argBeg;
    char** argEnd;

    char*              helpStr;
    struct ArgTracker* argTrackers;
};

// clang-format off
static struct ArgTracker ARG_TRACKER_CREATE[] = {
	{ "--zstdcl",         AT_COMPRESSION_LEVEL, 0, 0, "set compression level for ZSTD" },
	{ "--lz4cl",          AT_COMPRESSION_LEVEL, 0, 0, "set compression level for LZ4" },
	{ "--name",           AT_NAME,              1, 0, "set name for the next archive entry" },
	{ "--quiet",          AT_VERBOSITY,         0, 0, "disable stdout output (log not affected)" },
	{ "--verbose",        AT_VERBOSITY,         0, 0, "display useful statistics" },
	{ "--raw",            AT_FORMAT,            0, 0, "no   compression for next entries" },
	{ "--zstd",           AT_FORMAT,            0, 0, "ZSTD compression for next entries" },
	{ "--lz4",            AT_FORMAT,            0, 0, "LZ4  compression for next entries" },
	{ "--threads",        AT_THREADS,          -1, 99, "thread pool size. 0 singlethreaded. -1 auto" },
	{ "--parallel-reads", AT_PARALLEL_READS,    1, 99, "max number of file streams when thread pool enabled" },
	{ "--thread-memory",  AT_MEMORY_SIZE,       1, 64, "MB of memory allocated per thread. Threads can starve on low amount." },
	{ "--bsize",          AT_BLOCK_SIZE,        1, (BUNYAR_BLOCK_MAX_SIZE_MINUS_ONE + 1) / 1024, "size of compressed data block in KB" },
	{ "--hashmap",        AT_HASHMAP,           0, 0, "precompute hash table (enabled by default)" },
	{ "--no-hashmap",     AT_HASHMAP,           0, 0, "disable hash table precomputing" },
	{ "--optional",       AT_OPTIONAL,          0, 0, "keep going if next entries are missing" },
	{ "--required",       AT_OPTIONAL,          0, 0, "undo --optional" },
	{ "--help",           AT_HELP,              0, 0, "be provided with something that is useful or necessary in achieving" },
	{ NULL,               AT_UNRECOGNIZED,      0, 0, NULL },
};
static struct ArgTracker ARG_TRACKER_INSPECT[] = {
	{ "--blocks",     AT_BLOCKS,            0, 0, "gather compressed file block statistics" },
	{ "--help",       AT_HELP,              0, 0, "the act of doing something to make it easier to complete a task" },
	{ NULL,           AT_UNRECOGNIZED,      0, 0, NULL },
};
static struct ArgTracker ARG_TRACKER_EXTRACT[] = {
	{ "--keep-going", AT_CONTINUE_ON_ERROR, 0, 0, "continue on error" },
	{ "--quiet",      AT_VERBOSITY,         0, 0, "disable stdout output (log not affected)" },
	{ "--verbose",    AT_VERBOSITY,         0, 0, "display statistics" },
	{ "--help",       AT_HELP,              0, 0, "gain assistance or support to achieve goals" },
	{ NULL,           AT_UNRECOGNIZED,      0, 0, NULL },
};
static struct ArgTracker ARG_TRACKER_BENCHMARK[] = {
	{ "--key-count",  AT_KEY_COUNT,         0, 1000 * 1000 * 1000, "number of keys" },
	{ "--key-size",   AT_KEYSIZE,           1, 512, "size of key in bytes" },
	{ "--help",       AT_HELP,              0, 0, "get support or aid" },
	{ NULL,           AT_UNRECOGNIZED,      0, 0, NULL },
};
// clang-format on

static bool nextArg(struct BunyArToolCtx* ctx, char** out)
{
    *out = NULL;

    for (; ctx->argCur < ctx->argEnd; ++ctx->argCur)
    {
        char* a = *ctx->argCur;

        if (*a != '-')
        {
            if (!ctx->archivePathDontWanna && !ctx->archivePath)
            {
                ctx->archivePath = a;
                continue;
            }
            ++ctx->argCur;
            *out = a;
            return true;
        }

        // Split "--a=b"
        char* b = NULL;
        for (char* c = a + 1; *c; ++c)
        {
            if (*c != '=')
                continue;
            *c = 0;
            b = c + 1;
        }

        enum ArgType at = AT_UNRECOGNIZED;
        char         resolver = 0;
        long         value = 0;

        for (struct ArgTracker* p = ctx->argTrackers; p->name; ++p)
        {
            if (strcmp(p->name + 1, a + 1) != 0)
                continue;

            at = p->argType;
            resolver = p->name[2];

            if (p->min == p->max)
            {
                if (!b)
                    break;

                fprintf(stderr, "Unexpected value after '%s' argument '%s'.\n", b, a);
                return false;
            }

            if (!b)
            {
                ++ctx->argCur;
                if (ctx->argCur >= ctx->argEnd)
                {
                    fprintf(stderr, "Expected value after '%s' argument.\n", a);
                    return false;
                }

                b = *ctx->argCur;
            }

            if (p->min > p->max)
                break;

            char* end;
            value = strtol(b, &end, 10);
            if (*end || value < p->min || value > p->max)
            {
                fprintf(stderr,
                        "Bad value '%s' for argument '%s', "
                        "must be in range [%li;%li].\n",
                        b, a, p->min, p->max);
                return false;
            }

            break;
        }

        switch (at)
        {
        case AT_BLOCK_SIZE:
            ctx->blockSizeKb = (uint32_t)value;
            break;
        case AT_COMPRESSION_LEVEL:
            *(resolver == 'z' ? &ctx->zstdcl : &ctx->lz4cl) = (int)value;
            break;
        case AT_FORMAT:
            switch (resolver)
            {
            case 'r':
                ctx->format = BUNYAR_FILE_FORMAT_RAW;
                break;
            case 'l':
                ctx->format = BUNYAR_FILE_FORMAT_LZ4_BLOCKS;
                break;
            case 'z':
                ctx->format = BUNYAR_FILE_FORMAT_ZSTD_BLOCKS;
                break;
            }
            break;
        case AT_HASHMAP:
            ctx->hashMap = resolver != 'n';
            break;
        case AT_VERBOSITY:
            ctx->verbose = resolver == 'q' ? 0 : 2;
            break;
        case AT_OPTIONAL:
            ctx->optional = resolver == 'o';
            break;
        case AT_BLOCKS:
            ctx->inspectBlocks = true;
            break;
        case AT_NAME:
            ctx->outputName = b;
            break;
        case AT_HELP:
            ctx->help = true;
            break;
        case AT_CONTINUE_ON_ERROR:
            ctx->keepGoing = true;
            break;
        case AT_KEY_COUNT:
            ctx->keyCount = (size_t)value;
            break;
        case AT_KEYSIZE:
            ctx->keySize = (size_t)value;
            break;
        case AT_THREADS:
            ctx->threadCount = (int)value;
            break;
        case AT_PARALLEL_READS:
            ctx->parallelFileReads = (size_t)value;
            break;
        case AT_MEMORY_SIZE:
            ctx->MBPerThread = (size_t)value;
            break;
        case AT_UNRECOGNIZED:
        default:
            fprintf(stderr, "Unrecognized argument '%s'\n", a);
            return false;
        }
    }

    if (ctx->help)
        return false;

    if (!ctx->archivePathDontWanna && !ctx->archivePath)
    {
        fprintf(stderr, "Expected archive path argument\n");
        return false;
    }

    return true;
}

static void displayArgsHelp(struct ArgTracker* args)
{
    for (; args->help; ++args)
    {
        int nc = fprintf(stdout, "  %s", args->name);
        if (args->min != args->max)
        {
            if (args->min > args->max)
                nc += fprintf(stdout, "=string");
            else
                nc += fprintf(stdout, "=[%li;%li]", args->min, args->max);
        }

        const int width = 29;

        int spaces = width - nc;
        if (spaces <= 1)
            spaces = 1;

        fprintf(stdout, "%.*s%s\n", spaces, "                                                          ", args->help);
    }

    putc('\n', stdout);
}

static int bunyArToolCreate(struct BunyArToolCtx* ctx)
{
    {
        int min;
        int max;

        bunyArLibCompressionLevelLimits(BUNYAR_FILE_FORMAT_ZSTD_BLOCKS, &min, &max);
        ARG_TRACKER_CREATE[0].min = min;
        ARG_TRACKER_CREATE[0].max = max;

        bunyArLibCompressionLevelLimits(BUNYAR_FILE_FORMAT_LZ4_BLOCKS, &min, &max);
        ARG_TRACKER_CREATE[1].min = min;
        ARG_TRACKER_CREATE[1].max = max;
    }

    ctx->argTrackers = ARG_TRACKER_CREATE;

    // clang-format off
	ctx->helpStr =
	  "Create archive from the list of entries. Entries are directory or file paths.\n"
	  "\nUsage:\n\tcreate output_file --zstd Art --lz4 readme.txt --name backup /home/Downloads\n\n"
	  "Each entry has its own set of options, e.g. Art directory is compressed using ZSTD, while \"readme.txt\" and \"/home/Downloads\" entries are compressed using LZ4.\n\n"
	  "\"--name\" argument is used to set name for next entry, so files from \"/home/Downloads/\" are going to be located in the \"backup/\" archive directory.\n";
    // clang-format on

    struct BunyArLibCreateDesc info = { 0 };

    info.entries = tf_malloc(sizeof *info.entries * ctx->argCount);

    bool success = true;

    for (;;)
    {
        char* arg;
        if (!nextArg(ctx, &arg))
        {
            success = false;
            break;
        }

        if (arg == NULL)
            break;

        if (ctx->help)
        {
            success = false;
            break;
        }

        struct BunyArLibEntryCreateDesc* entry = info.entries + (info.entryCount++);

        entry->inputRd = TF_RD;
        entry->inputPath = arg;
        entry->outputName = ctx->outputName;
        entry->format = ctx->format;
        entry->blockSizeKb = ctx->blockSizeKb;
        entry->optional = ctx->optional;

        ctx->outputName = NULL;

        if (!entry->outputName && ctx->outputNameCutLength)
            entry->outputName = entry->inputPath + ctx->outputNameCutLength;

        switch (entry->format)
        {
        case BUNYAR_FILE_FORMAT_LZ4_BLOCKS:
            entry->compressionLevel = ctx->lz4cl;
            break;
        case BUNYAR_FILE_FORMAT_ZSTD_BLOCKS:
            entry->compressionLevel = ctx->zstdcl;
            break;
        case BUNYAR_FILE_FORMAT_RAW:
        default:
            entry->compressionLevel = 0;
        }
    }

    if (success)
    {
        info.skipHashTable = !ctx->hashMap;
        info.verbose = ctx->verbose;

        info.maxParallelFileReads = ctx->parallelFileReads;
        info.threadPoolSize = ctx->threadCount;
        info.memorySizePerThread = ctx->MBPerThread * 1024 * 1024;

        success = bunyArLibCreate(TF_RD, ctx->archivePath, &info);
    }

    tf_free(info.entries);

    return success ? 0 : -1;
}

static int bunyArToolInspect(struct BunyArToolCtx* ctx)
{
    ctx->argTrackers = ARG_TRACKER_INSPECT;

    // clang-format off
	ctx->helpStr =
	  "Show files from archive with compression statistics.\n"
	  "\nUsage:\n\tinspect archive_file --blocks\n";
    // clang-format on

    for (;;)
    {
        char* arg;
        if (!nextArg(ctx, &arg))
            return -1;

        if (arg == NULL)
            break;

        fprintf(stderr, "Unexpected argument '%s'\n", arg);
        return -1;
    }

    struct ArchiveOpenDesc adesc = { 0 };

    adesc.disableHashTable = true;
    adesc.validation = true;

    IFileSystem archiveFs;
    if (!fsArchiveOpen(TF_RD, ctx->archivePath, &adesc, &archiveFs))
    {
        fprintf(stderr, "Failed to open archive %s\n", ctx->archivePath);
        return -1;
    }

    struct BunyArDescription archiveInfo;
    fsArchiveGetDescription(&archiveFs, &archiveInfo);

    for (uint64_t i = 0; i < archiveInfo.nodeCount; ++i)
    {
        struct BunyArNodeDescription node;
        fsArchiveGetNodeDescription(&archiveFs, i, &node);

        fprintf(stdout, "'%s'\n|- %s %s -> %s (x%.2f)\n", node.name, bunyArFormatName(node.format), humanReadableSize(node.fileSize).str,
                humanReadableSize(node.compressedSize).str, (double)node.fileSize / (double)node.compressedSize);

        if (ctx->inspectBlocks && node.format != BUNYAR_FILE_FORMAT_RAW)
        {
            FileStream fs;
            if (!fsIoOpenByUid(&archiveFs, i, FM_READ, &fs))
            {
                fprintf(stderr, "Failed to open node '%s' for inspection\n", node.name);
                putc('\n', stdout);
                continue;
            }

            struct BunyArBlockFormatHeader blocksHeader;
            const BunyArBlockPointer*      blocks;

            if (fsArchiveGetFileBlockMetadata(&fs, &blocksHeader, &blocks))
            {
                fprintf(stdout, "|- ");
                bunyArLibPrintBlockAnalysis(&blocksHeader, blocks);
                putc('\n', stdout);
            }
            else
            {
                fprintf(stdout, "|- failed to retreive blocks metadata");
            }

            fsCloseStream(&fs);
        }

        putc('\n', stdout);
    }

    fsArchiveClose(&archiveFs);
    return 0;
}

static int bunyArToolExtract(struct BunyArToolCtx* ctx)
{
    ctx->argTrackers = ARG_TRACKER_EXTRACT;

    // clang-format off
	ctx->helpStr =
	  "Extract all files or specific files.\n"
	  "\nUsage:\n\textract archive_file\n\textract archive_file Meshes/body.gltf TestDoc.txt\n";
    // clang-format on

    struct BunyArLibExtractDesc desc = { 0 };

    desc.fileNames = tf_malloc(sizeof *desc.fileNames * ctx->argCount);

    bool success = true;

    const char* output = "";
    for (;;)
    {
        char* arg;
        if (!nextArg(ctx, &arg))
        {
            success = false;
            break;
        }

        if (arg == NULL)
            break;

        if (output)
            desc.fileNames[desc.fileNameCount++] = arg;
        else
            output = arg;
    }

    desc.verbose = ctx->verbose;
    desc.continueOnError = ctx->keepGoing;

    struct ArchiveOpenDesc adesc = { 0 };

    adesc.disableHashTable = desc.fileNameCount == 0;
    adesc.validation = true;

    IFileSystem archiveFs = { 0 };
    if (success && !fsArchiveOpen(TF_RD, ctx->archivePath, &adesc, &archiveFs))
    {
        fprintf(stderr, "Failed to open archive %s\n", ctx->archivePath);
        success = false;
    }

    if (success)
        success = bunyArLibExtract(&archiveFs, TF_RD, output, &desc);

    fsArchiveClose(&archiveFs);

    tf_free(desc.fileNames);

    return success ? 0 : -1;
}

static int bunyArToolBenchmark(struct BunyArToolCtx* ctx)
{
    ctx->archivePathDontWanna = true;
    ctx->argTrackers = ARG_TRACKER_BENCHMARK;

    // clang-format off
	ctx->helpStr =
	  "Hash table benchmark.\n"
	  "\nUsage:\n\tbenchmark --key-size=8 --key-count=100000000\n";
    // clang-format on

    for (;;)
    {
        char* arg;
        if (!nextArg(ctx, &arg))
            return -1;

        if (arg == NULL)
            break;

        fprintf(stderr, "Unexpected argument '%s'\n", arg);
        return -1;
    }

    return bunyArLibHashTableBenchmarks(ctx->keyCount, ctx->keySize) ? 0 : -1;
}

static inline bool isRootPath(char* path)
{
#if defined(_WINDOWS)
    return path[0] && path[1] == ':' && path[2] == DIR_SEP;
#else
    return path[0] == DIR_SEP;
#endif
}

// Mac has no support for drag&drop,
// so there are only tricks for Windows and Linux
static bool tryDragNdrop(struct BunyArToolCtx* ctx, int* res, char** freeMe)
{
    *freeMe = NULL;

    char** paths = ctx->argBeg;
    while (!isRootPath(*paths))
    {
        ++paths;
        if (paths >= ctx->argEnd)
            return false;
    }

    for (char** ac = paths; *ac; ++ac)
    {
        if (!isRootPath(*ac))
        {
            fprintf(stderr,
                    "Unexpected path. All paths must be full paths for drag&drop. "
                    "'%s'",
                    *ac);
            return true;
        }
    }

    size_t pathCount = (size_t)(ctx->argEnd - paths);

    char* firstFile = paths[0];
    // drag-n-drop
    bool  exist;
    bool  isDir;
    bool  isFile;
    if (!fsCheckPath(TF_RD, firstFile, &exist, &isDir, &isFile))
        return false;
    if (!exist)
    {
        fprintf(stderr, "Entry is not exist: '%s'\n", firstFile);
        return false;
    }

    size_t lenOfFirstFile = strlen(firstFile);

    size_t      pathLen = 0;
    const char* end = firstFile + lenOfFirstFile;
    for (; end > firstFile; --end)
    {
        if (end[-1] == DIR_SEP)
        {
            pathLen = (size_t)(end - firstFile);
            break;
        }
    }

    if (!isDir && pathCount == 1)
    {
        struct ArchiveOpenDesc info = { 0 };

        info.tryMode = true;
        info.disableHashTable = true;

        IFileSystem fs;
        if (fsArchiveOpen(TF_RD, firstFile, &info, &fs))
        {
            struct BunyArDescription inf;
            fsArchiveGetDescription(&fs, &inf);

            const char* dstDir = NULL;
            const char* dstEntry = NULL;

            if (inf.nodeCount == 0)
            {
                fprintf(stdout, "Archive is empty\n");
                fsArchiveClose(&fs);
                return true;
            }
            else if (inf.nodeCount == 1)
            {
                char* outName = tf_malloc(pathLen + 1);
                memcpy(outName, firstFile, pathLen);
                outName[pathLen] = 0;
                *freeMe = outName;
                dstDir = outName;

                struct BunyArNodeDescription nodeInfo;
                fsArchiveGetNodeDescription(&fs, 0, &nodeInfo);
                dstEntry = nodeInfo.name;
                if (!fsCheckPath(TF_RD, nodeInfo.name, &exist, &isDir, &isFile))
                    return false;
            }
            else
            {
                char* outName = tf_malloc(lenOfFirstFile + 6);
                memcpy(outName, firstFile, lenOfFirstFile + 1);
                *freeMe = outName;
                dstDir = outName;

                dstEntry = outName;

                if (lenOfFirstFile > 6 && memcmp(firstFile + lenOfFirstFile - 5, ".buny", 5) == 0)
                {
                    outName[lenOfFirstFile - 5] = 0;
                    if (!fsCheckPath(TF_RD, outName, &exist, &isDir, &isFile))
                        return false;
                }
                else
                {
                    memcpy(outName + lenOfFirstFile, ".data", 6);
                    if (!fsCheckPath(TF_RD, outName, &exist, &isDir, &isFile))
                        return false;
                }
            }

            if (exist)
            {
                fprintf(stderr, "%s already exist: '%s'\n", isDir ? "Directory" : "File", dstEntry);
                *res = -1;
                return true;
            }

            struct BunyArLibExtractDesc extractInfo = { 0 };

            extractInfo.verbose = 1;

            *res = bunyArLibExtract(&fs, TF_RD, dstDir, &extractInfo);

            fsArchiveClose(&fs);
            return true;
        }
    }

    for (size_t i = 1; i < pathCount; ++i)
    {
        if (strncmp(firstFile, paths[i], pathLen) != 0)
        {
            fprintf(stderr, "Creating archive from entries from different paths is not "
                            "supported\n");
            *res = -1;
            return true;
        }
    }

    if (pathCount == 1 && isDir)
    {
        ctx->outputName = ".";
    }

    const char extension[] = ".buny.tmp";
    char*      outName = tf_malloc(lenOfFirstFile + sizeof extension);
    memcpy(outName, firstFile, lenOfFirstFile);
    memcpy(outName + lenOfFirstFile, extension, sizeof extension);

    *freeMe = outName;

    if (!fsCheckPath(TF_RD, outName, &exist, &isDir, &isFile))
        return false;

    if (!exist)
    {
        outName[lenOfFirstFile + 5] = 0;
        if (!fsCheckPath(TF_RD, outName, &exist, &isDir, &isFile))
            return false;
    }

    if (exist)
    {
        fprintf(stderr, "File already exist:\n'%s'\n", outName);
        *res = -1;
        return true;
    }

    outName[lenOfFirstFile + 5] = '.';

    ctx->archivePath = outName;
    ctx->outputNameCutLength = pathLen;

#if !defined(_WINDOWS) // No window opened on Linux
    ctx->verbose = 0;
#endif

    *res = bunyArToolCreate(ctx);

    if (*res == 0)
    {
        char* name = tf_malloc(lenOfFirstFile + 6);
        memcpy(name, firstFile, lenOfFirstFile);
        memcpy(name + lenOfFirstFile, ".buny", 6);
        if (fsRenameFile(TF_RD, outName, name))
        {
            fprintf(stdout, "Renamed to '%s'\n", name);
        }
        tf_free(name);
    }
    else
    {
        if (fsRemoveFile(TF_RD, outName))
            fprintf(stdout, "File removed '%s'\n", outName);
    }

    return true;
}

static int bunyArTool(int argCount, char** args)
{
#if defined(_WINDOWS)
    // fixes broken behaviour of utf8 functions (e.g. mbstowcs_s)
    if (setlocale(LC_ALL, ".UTF-8") == NULL)
    {
        printf("Error setting locale \".UTF-8\"\n");
    }
#endif

    if (argCount < 2)
    {
        putc('\n', stdout);
        fprintf(stdout, "The-Forge Buny Archive tool. Version 0.\n");
    DEFAULT_OUTPUT:
        fprintf(stdout, "\nUsage:\n\t%s command --help\n\n", BUNYAR_TOOL_NAME);
        fprintf(stdout, "Commands:\n");
        fprintf(stdout, "\tcreate      Create archive\n");
        fprintf(stdout, "\tinspect     Lookup archive content\n");
        fprintf(stdout, "\textract     Extract archive\n");
        fprintf(stdout, "\tbenchmark   Run benchmarks\n");
        putc('\n', stdout);
        return argCount != 1;
    }

    struct BunyArToolCtx ctx = { 0 };

    struct BunyArLibEntryCreateDesc defaults = BUNYAR_LIB_FUNC_CREATE_DEFAULT_ENTRY_DESC;

    char* cmd = args[1];

    ctx.verbose = 1;
    ctx.hashMap = true;

    ctx.blockSizeKb = defaults.blockSizeKb;
    ctx.format = defaults.format;
    ctx.zstdcl = defaults.compressionLevel;
    ctx.lz4cl = defaults.compressionLevel;
    ctx.optional = defaults.optional;

    ctx.threadCount = -1;

    ctx.keyCount = 10000000;
    ctx.keySize = 8;

    ctx.argBeg = args + 2;
    ctx.argEnd = args + argCount;
    ctx.argCur = ctx.argBeg;
    ctx.argCount = (size_t)(ctx.argEnd - ctx.argBeg);

    int res = -1;
    if (strcmp(cmd, "create") == 0)
        res = bunyArToolCreate(&ctx);
    else if (strcmp(cmd, "inspect") == 0)
        res = bunyArToolInspect(&ctx);
    else if (strcmp(cmd, "extract") == 0)
        res = bunyArToolExtract(&ctx);
    else if (strcmp(cmd, "benchmark") == 0)
        res = bunyArToolBenchmark(&ctx);
    else
    {
        ctx.argBeg = args + 1;
        ctx.argEnd = args + argCount;
        ctx.argCur = ctx.argBeg;
        ctx.argCount = (size_t)(ctx.argEnd - ctx.argBeg);

        char* mem = NULL;
        bool  dnd = tryDragNdrop(&ctx, &res, &mem);
        tf_free(mem);
        if (dnd)
        {
#if defined(_WINDOWS) // No window opened on Linux
            fputs("Press enter to clow window: ", stdout);
            fflush(stdout);
            fflush(stderr);
            getc(stdin);
#endif
            return res;
        }

        if (cmd[0] == '-')
        {
            fprintf(stderr, "Expected command or list of full file paths "
#if defined(_WINDOWS)
                            "(\"c:\\b\\a\")"
#else
                            "(\"/a/b/c\")"
#endif
                            "\n");
        }
        else
        {
            fprintf(stderr, "Unknown command '%s'\n", cmd);
        }
        goto DEFAULT_OUTPUT;
    }

    if (ctx.help)
    {
        if (ctx.helpStr)
            fprintf(stdout, "\n%s\n", ctx.helpStr);
        displayArgsHelp(ctx.argTrackers);
    }

    return res;
}

int main(int argCount, char** args)
{
    initLog(NULL, eALL);
    if (!initMemAlloc(NULL))
        return EXIT_FAILURE;

    FileSystemInitDesc fsInfo = { 0 };
    fsInfo.pResourceMounts[0] = ".";

    if (!initFileSystem(&fsInfo))
    {
        fprintf(stderr, "Filesystem failed to initialize\n");
        return -1;
    }

    fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, TF_RD, "");

    int res = bunyArTool(argCount, args);

    exitFileSystem();
    exitMemAlloc();
    exitLog();

    return res;
}
