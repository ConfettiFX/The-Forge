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

#include "../../Utilities/Interfaces/IToolFileSystem.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define BUNYAR_LIB_COMPRESSION_LEVEL_DEFAULT INT_MIN
#define BUNYAR_LIB_BLOCK_SIZE_KB_DEFAULT     UINT32_MAX
#define BUNYAR_LIB_FORMAT_DEFAULT            BUNYAR_FILE_FORMAT_LZ4_BLOCKS

    struct BunyArLibEntryCreateDesc
    {
        // where read from
        ResourceDirectory inputRd;
        // where read from
        const char*       inputPath;

        // name to save in archive. If NULL, uses inputPath
        const char* outputName;

        enum BunyArFileFormat format;

        // multiple of KB, must be <= (BUNYAR_BLOCK_MAX_SIZE_MINUS_ONE + 1) / 1024
        uint32_t blockSizeKb; // BUNYAR_LIB_BLOCK_SIZE_KB_DEFAULT

        // negative values are compression speed acceleration levels
        // positive values improve compression ratio, but decreases speed
        // use bunyArLibCompressionLevelLimits to get valid bounds
        int compressionLevel; // BUNYAR_LIB_COMPRESSION_LEVEL_DEFAULT

        // skip entry content if it is missing
        bool optional;

#if defined BUNYAR_LIB_INTERNAL
        // Required because of qsort undefined behaviour
        uint64_t index;
#else
    // Write whatever funny things you want here, it does not matter
    uint64_t reservedValue;
#endif
    };

    struct BunyArLibCreateDesc
    {
        uint64_t                         entryCount;
        struct BunyArLibEntryCreateDesc* entries;

        bool     skipHashTable;
        // larger value, more details
        unsigned verbose;

        // if < 0, uses getNumCPUCores()
        // if = 0, uses single-threaded code path
        // if > 0, threadPoolSize + manager thread + writer thread
        int threadPoolSize;

        // Maximum file read streams. Default is 4.
        // Clamps to file count and thread count.
        size_t maxParallelFileReads;

        // Size allocated to file raw+compressed blocks per thread.
        // Recommended 4MB per thread required to keep all CPUs busy.
        // Minimum is 4KB
        // If 0, it sets to default 4MB
        size_t memorySizePerThread;
    };

    static const struct BunyArLibEntryCreateDesc BUNYAR_LIB_FUNC_CREATE_DEFAULT_ENTRY_DESC = {
        (ResourceDirectory)0,
        NULL,
        NULL,
        BUNYAR_LIB_FORMAT_DEFAULT,
        BUNYAR_LIB_BLOCK_SIZE_KB_DEFAULT,
        BUNYAR_LIB_COMPRESSION_LEVEL_DEFAULT,
        false,
        0,
    };

    void bunyArLibCompressionLevelLimits(enum BunyArFileFormat format, int* min, int* max);

    bool bunyArLibCreate(ResourceDirectory rd, const char* dstPath, const struct BunyArLibCreateDesc* desc);

    struct BunyArLibExtractDesc
    {
        // if fileNameCount is 0, all files are extracted
        size_t fileNameCount;
        char** fileNames; // const

        // 0 error log
        // 1 ^ + print file name before extracting
        // 2 ^ + print file details before extracting
        unsigned verbose;

        // Try all files even after failing extracting anyone,
        // e.g. if one from "fileNames" is missing, extract others anyway
        bool continueOnError;
    };

    bool bunyArLibExtract(struct IFileSystem* archiveFs, ResourceDirectory rd, const char* dstPath,
                          const struct BunyArLibExtractDesc* desc);

    void bunyArLibPrintBlockAnalysis(const struct BunyArBlockFormatHeader* header, const BunyArBlockPointer* blocks);

    bool bunyArLibHashTableBenchmarks(size_t keyCount, size_t keySize);

#ifdef __cplusplus
}
#endif
