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

#pragma once
#include "../../Application/Config.h"

#include "../../OS/Interfaces/IOperatingSystem.h"

// IOS Simulator paths can get a bit longer then 256 bytes
#ifdef TARGET_IOS_SIMULATOR
#define FS_MAX_PATH 320
#else
#define FS_MAX_PATH 512
#endif
#define PATHSTATEMENT_FILE_NAME "PathStatement.txt"

struct bstring;

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ResourceDirectory
    {
        /// The main application's shader binaries directory
        RD_SHADER_BINARIES = 0,

        RD_PIPELINE_CACHE,
        /// The main application's texture source directory (TODO processed texture folder)
        RD_TEXTURES,
        RD_COMPILED_MATERIALS,
        RD_MESHES,
        RD_FONTS,
        RD_ANIMATIONS,
        RD_AUDIO,
        RD_GPU_CONFIG,
        RD_LOG,
        RD_SCRIPTS,
        RD_SCREENSHOTS,
        RD_DEBUG,
#if defined(ANDROID)
        // #TODO: Add for others if necessary
        RD_SYSTEM,
#endif
        RD_OTHER_FILES,

        // Libraries can have their own directories.
        // Up to 100 libraries are supported.
        ____rd_lib_counter_begin = RD_OTHER_FILES + 1,

        // Add libraries here
        RD_MIDDLEWARE_0 = ____rd_lib_counter_begin,
        RD_MIDDLEWARE_1,
        RD_MIDDLEWARE_2,
        RD_MIDDLEWARE_3,
        RD_MIDDLEWARE_4,
        RD_MIDDLEWARE_5,
        RD_MIDDLEWARE_6,
        RD_MIDDLEWARE_7,
        RD_MIDDLEWARE_8,
        RD_MIDDLEWARE_9,
        RD_MIDDLEWARE_10,
        RD_MIDDLEWARE_11,
        RD_MIDDLEWARE_12,
        RD_MIDDLEWARE_13,
        RD_MIDDLEWARE_14,
        RD_MIDDLEWARE_15,

        ____rd_lib_counter_end = ____rd_lib_counter_begin + 99 * 2,
        RD_COUNT
    } ResourceDirectory;

    typedef enum SeekBaseOffset
    {
        SBO_START_OF_FILE = 0,
        SBO_CURRENT_POSITION,
        SBO_END_OF_FILE,
    } SeekBaseOffset;

    typedef enum FileMode
    {
        // Get read access for file. Error if file not exist.
        FM_READ = 1 << 0,

        // Get write access for file. File is created if not exist.
        FM_WRITE = 1 << 1,

        // Set initial seek position to the end of file.
        FM_APPEND = 1 << 2,

        // Read access for other processes.
        // Note: flag is required for Windows&Xbox.
        //       On other platforms read access is always available.
        FM_ALLOW_READ = 1 << 4,

        // RW mode
        FM_READ_WRITE = FM_READ | FM_WRITE,

        // W mode and set position to the end
        FM_WRITE_APPEND = FM_WRITE | FM_APPEND,

        // R mode and set position to the end
        FM_READ_APPEND = FM_READ | FM_APPEND,

        // RW mode and set position to the end
        FM_READ_WRITE_APPEND = FM_READ | FM_APPEND,

        // -- mode and -- and also read access for other processes.
        FM_WRITE_ALLOW_READ = FM_WRITE | FM_ALLOW_READ,
        FM_READ_WRITE_ALLOW_READ = FM_READ_WRITE | FM_ALLOW_READ,
        FM_WRITE_APPEND_ALLOW_READ = FM_WRITE_APPEND | FM_ALLOW_READ,
        FM_READ_WRITE_APPEND_ALLOW_READ = FM_READ_WRITE_APPEND | FM_ALLOW_READ,
    } FileMode;

    typedef struct IFileSystem IFileSystem;

    struct FileStreamUserData
    {
        uintptr_t data[6];
    };

    typedef struct ResourceDirectoryInfo
    {
        IFileSystem* pIO;
        char         mPath[FS_MAX_PATH];
        bool         mBundled;
    } ResourceDirectoryInfo;

    /// After stream is opened, only FileStream::pIO must be used for this stream.
    /// Example:
    ///   io->Open(&stream); // stream is opened
    ///   io->Read(&stream, ...); // bug, potentially uses wrong io on wrong stream.
    ///   stream.pIO->Read(&stream, ...); // correct
    /// The reason for this is that IFileSystem::Open can open stream using another
    /// IFileSystem handle.
    ///
    /// It is best to use IFileSystem IO shortcuts "fsReadFromStream(&stream,...)"
    typedef struct FileStream
    {
        IFileSystem*              pIO;
        FileMode                  mMode;
        struct FileStreamUserData mUser; // access to this field is IO exclusive
    } FileStream;

    typedef struct FileSystemInitDesc
    {
        const char* pAppName;
        void*       pPlatformData;
        // should be true for tools it will skip using
        // PathStatement file and set resource dirs manually
        // using fsSetResourceDirectory
        // in Consoles and Phones it will be ignored
        bool        mIsTool;
    } FileSystemInitDesc;

    struct IFileSystem
    {
        bool (*Open)(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

        /// Closes and invalidates the file stream.
        bool (*Close)(FileStream* pFile);

        /// Returns the number of bytes read.
        size_t (*Read)(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);

        /// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
        /// Returns the number of bytes written.
        size_t (*Write)(FileStream* pFile, const void* sourceBuffer, size_t byteCount);

        /// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
        bool (*Seek)(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);

        /// Gets the current seek position in the file.
        ssize_t (*GetSeekPosition)(FileStream* pFile);

        /// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
        ssize_t (*GetFileSize)(FileStream* pFile);

        /// Flushes all writes to the file stream to the underlying subsystem.
        bool (*Flush)(FileStream* pFile);

        /// Returns whether the current seek position is at the end of the file stream.
        bool (*IsAtEnd)(FileStream* pFile);

        // Acquire unique file identifier.
        // Only Archive FS supports it currently.
        bool (*GetFileUid)(IFileSystem* pIO, ResourceDirectory rd, const char* name, uint64_t* outUid);

        // Open file using unique identifier. Use GetFileUid to get uid.
        bool (*OpenByUid)(IFileSystem* pIO, uint64_t uid, FileMode fm, FileStream* pOut);

        // Creates virtual address space of file.
        // When memory mapping is done, file can be accessed just like an array.
        // This is more efficient than using "FILE" stream.
        // Not all platforms are supported.
        // Use fsStreamWrapMemoryMap for strong cross-platform compatibility.
        // This function does read-only memory map.
        bool (*MemoryMap)(FileStream* fs, size_t* outSize, void const** outData);

        // getSystemHandle
        void* (*GetSystemHandle)(FileStream* fs);

        void* pUser;
    };

    /// Default file system using C File IO or Bundled File IO (Android) based on the ResourceDirectory
    FORGE_API extern IFileSystem* pSystemFileIO;
    /************************************************************************/
    // MARK: - Initialization
    /************************************************************************/
    /// Initializes the FileSystem API
    /// utlize PathStatement.txt file in Art directory
    /// unless FileSystemInitDesc::toolsFilesystem = true
    FORGE_API bool                initFileSystem(FileSystemInitDesc* pDesc);

    /// Frees resources associated with the FileSystem API
    FORGE_API void exitFileSystem(void);

    /************************************************************************/
    // MARK: - Archive file system
    /************************************************************************/

    struct ArchiveOpenDesc
    {
        // Binary search "strcmp" is used as an alternative to hash table.
        // Increases archive opening speed and decreases used memory.
        // Useful if user won't resolve filenames (GetFileUid, Open),
        // and will use UIDs instead (OpenByUid).
        bool disableHashTable;

        // Enable validation features, e.g.
        //      hashtable verification
        //      file name normalization
        //
        // If issues are found, archive fs can fix them most of the times.
        // If validation is disabled, those issues are left undetected.
        //
        // Always enabled with FORGE_DEBUG macro.
        //
        // Use it for developer tools. Don't use it for shipping.
        bool validation;

        // Makes archive stream thread-safe
        // It allows to read several files from archive asynchronously.
        // Not used for fsArchiveOpenFromMemory
        //
        // Allows: (if this flag is set)
        // Thread1: reads file "A"
        // Thread2: reads file "B"
        //
        // Does not allow: (do not do this)
        // Thread1: reads file "A"
        // Thread2: reads file "A"
        bool protectStreamCriticalSection;

        // Do not log errors if archive header is wrong
        // Set this flag if your thoughts are:
        // "lets try to open file and see if this is an archive or not"
        bool tryMode;

        // Try to memory map stream using fsStreamMemoryMap
        bool mmap;
    };

    /// 'desc' can be NULL
    FORGE_API bool fsArchiveOpen(ResourceDirectory rd, const char* fileName, const struct ArchiveOpenDesc* desc, IFileSystem* out);

    /// This function is useful to use arbitrary stream source as an archive stream.
    /// Only (*Seek) and (*Read) functions are required.
    ///
    /// 'desc' can be NULL
    /// 'stream' is owned by user, must be valid until fsArchiveClose
    ///          do not use stream while archive is opened
    FORGE_API bool fsArchiveOpenFromStream(FileStream* stream, const struct ArchiveOpenDesc* desc, IFileSystem* out);

    /// Archive can be opened on "read from memory" mode.
    /// Benifits:
    /// - no need for ArchiveOpenDesc::protectStreamCriticalSection
    /// - no filesystem overhead
    /// - less buffering during decompression
    ///
    /// Recommended for mmap-ed memory.
    ///
    /// 'desc' can be NULL
    /// 'm' is owned by user, must be valid until fsArchiveClose
    FORGE_API bool fsArchiveOpenFromMemory(uint64_t msize, const void* m, const struct ArchiveOpenDesc* desc, IFileSystem* out);

    FORGE_API bool fsArchiveClose(IFileSystem* pArchive);

    /************************************************************************/
    // MARK: - File IO
    /************************************************************************/

    /// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
    /// to read from or modify the file. May return NULL if the file could not be opened.
    FORGE_API bool fsOpenStreamFromPath(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

    /// Opens a memory buffer as a FileStream, returning a stream that must be closed with `fsCloseStream`.
    /// Use 'fsStreamMemoryMap' to do the opposite.
    FORGE_API bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut);

    FORGE_API bool fsFindStream(FileStream* fs, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition);
    FORGE_API bool fsFindReverseStream(FileStream* fs, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition);

    /// Checks if stream is a standard system stream
    FORGE_API bool fsIsSystemFileStream(FileStream* fs);
    /// Checks if stream is a memory stream
    FORGE_API bool fsIsMemoryStream(FileStream* fs);

    /// symbolsCount can be SIZE_MAX, then reads until the end of file
    /// appends '\0' to the end of string
    FORGE_API size_t fsReadBstringFromStream(FileStream* stream, struct bstring* pStr, size_t symbolsCount);

    /// Wraps stream into new memory stream using fsStreamMemoryMap
    /// returns true: old stream is wrapped by new one with new IO.
    /// returns false: stream is unaffected.
    /// In both cases stream stays in valid state.
    /// fsCloseStream(FileStream*) takes care of cleaning wrapped stream.
    /// So checking return value is optional.
    FORGE_API bool fsStreamWrapMemoryMap(FileStream* fs);

    FORGE_API void* fsGetSystemHandle(FileStream* fs);
    /************************************************************************/
    // MARK: - IFileSystem IO shortcuts
    /************************************************************************/

    /// Closes and invalidates the file stream.
    static inline bool fsCloseStream(FileStream* fs)
    {
        if (!fs->pIO)
            return true;
        bool success = fs->pIO->Close(fs);
        memset(fs, 0, sizeof *fs);
        return success;
    }

    /// Returns the number of bytes read.
    static inline size_t fsReadFromStream(FileStream* fs, void* pOutputBuffer, size_t bufferSizeInBytes)
    {
        return fs->pIO->Read(fs, pOutputBuffer, bufferSizeInBytes);
    }

    /// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
    /// Returns the number of bytes written.
    static inline size_t fsWriteToStream(FileStream* fs, const void* pSourceBuffer, size_t byteCount)
    {
        if (!fs->pIO->Write)
            return 0;
        return fs->pIO->Write(fs, pSourceBuffer, byteCount);
    }

    /// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
    static inline bool fsSeekStream(FileStream* fs, SeekBaseOffset baseOffset, ssize_t seekOffset)
    {
        return fs->pIO->Seek(fs, baseOffset, seekOffset);
    }

    /// Gets the current seek position in the file.
    static inline ssize_t fsGetStreamSeekPosition(FileStream* fs) { return fs->pIO->GetSeekPosition(fs); }

    /// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
    static inline ssize_t fsGetStreamFileSize(FileStream* fs) { return fs->pIO->GetFileSize(fs); }

    /// Flushes all writes to the file stream to the underlying subsystem.
    static inline bool fsFlushStream(FileStream* fs)
    {
        if (!fs->pIO->Flush)
            return false;
        return fs->pIO->Flush(fs);
    }

    /// Returns whether the current seek position is at the end of the file stream.
    static inline bool fsStreamAtEnd(FileStream* fs) { return fs->pIO->IsAtEnd(fs); }

    static inline bool fsIoGetFileUid(IFileSystem* pIO, ResourceDirectory rd, const char* fileName, uint64_t* outUid)
    {
        if (!pIO->GetFileUid)
            return false;
        return pIO->GetFileUid(pIO, rd, fileName, outUid);
    }

    static inline bool fsIoOpenByUid(IFileSystem* pIO, uint64_t index, FileMode mode, FileStream* pOutStream)
    {
        if (!pIO->OpenByUid)
            return false;
        return pIO->OpenByUid(pIO, index, mode, pOutStream);
    }

    static inline bool fsStreamMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
    {
        if (!fs->pIO->MemoryMap)
            return false;
        return fs->pIO->MemoryMap(fs, outSize, outData);
    }

    /************************************************************************/
    // MARK: - Directory queries
    /************************************************************************/

    /// Sets the relative path for `resourceDir` from `mount` to `bundledFolder`.
    /// The `resourceDir` will making use of the given IFileSystem `pIO` file functions.
    /// When `mount` is set to `RM_CONTENT` for a `resourceDir`, this directory is marked as a bundled resource folder.
    /// Bundled resource folders should only be used for Read operations.
    /// NOTE: A `resourceDir` can only be set once.
    FORGE_API void fsSetPathForResourceDir(IFileSystem* pIO, ResourceDirectory resourceDir, const char* bundledFolder);

    /************************************************************************/
    // MARK: - Buny Archive format definitions
    /************************************************************************/

    // Buny Archive File structure description:
    // First bytes are BunyArHeader with magic values and metadata location pointers
    // Metadata consist of following units:
    //    archive nodes (file entries)
    //    block of utf8 strings, referenced by nodes
    //    precomputed hash table (optional)
    //
    // Archive node contains file location within archive and other details
    //
    // Archive supports several compression formats.

    enum BunyArFileFormat
    {
        BUNYAR_FILE_FORMAT_RAW = 0,

        // File is splitted to N X-size blocks.
        // Leading data in a "blocks" format is represented by
        // BunyArBlockFormatHeader and BunyArBlockPointer table.
        // Table allows to seek through file quickly, while block size
        // is large enough for compression to be effective.
        BUNYAR_FILE_FORMAT_LZ4_BLOCKS = 3,
        BUNYAR_FILE_FORMAT_ZSTD_BLOCKS = 5,
    };

    static const uint8_t BUNYAR_MAGIC[16] = {
        'B', 'u', 'n', 'y', 'A', 'r', 'c', 'h', // BunyArch
        'T', 'h', 'e', 'F', 'o', 'r', 'g', 'e', // TheForge
    };

// Artificial limit, to follow FS_MAX_PATH.
// Avoid using FS_MAX_PATH here because it varies, usually it is equal to 512
// FileSystem can not open file if name length is beyond limit.
// = 512 with trailing 0 at the end of str
#define BUNYAR_FILE_NAME_LENGTH_MAX 511

    // points to location within archive
    struct BunyArPointer64
    {
        uint64_t offset; // exact offset
        uint64_t size;   // exact size
    };

    // points to relative location
    struct BunyArPointer32
    {
        uint32_t offset; // relative offset
        uint32_t size;   // exact size
    };

    // Reader can still use archive,
    // if condition "compatible <= X <= actual" is met, where X is reader version.
    struct BunyArVersion
    {
        uint32_t compatible; // backwards compatible version
        uint32_t actual;     // archive version
    };

    struct BunyArHeader
    {
        // ARCHIVE_MAGIC
        uint8_t magic[sizeof BUNYAR_MAGIC];

        struct BunyArVersion version;

        // 0, reserved
        uint64_t flags;

        // nodeCount = nodesPointer.size / sizeof(BunyArNode)
        struct BunyArPointer64 nodesPointer;

        // required size > 0 if nodesPoinster.size > 0
        // size of 0 allowed for archive without entries
        struct BunyArPointer64 namesPointer;

        // Location of BunyArHashTable.
        // Hash table present, if size >= sizeof(BunyArHashTable)
        struct BunyArPointer64 hashTablePointer;

        // header can be extended in the future by new variables or pointers
    };

    struct BunyArNode
    {
        // BunyArFileFormat
        uint64_t format;

        // Original size of file compressed into an archive.
        // Also exact size of the file after reading from an archive
        uint64_t originalFileSize;

        // location of name relative to location of BunyArHeader::namesPointer
        // name is utf8 null-terminated string
        // size must not count trailing 0
        // size must be <= BUNYAR_FILE_NAME_LENGTH_MAX
        struct BunyArPointer32 namePointer;

        // location of compressed file
        struct BunyArPointer64 filePointer;
    };

    struct BunyArBlockFormatHeader
    {
        // size of all uncompressed blocks except the last one
        uint64_t blockSize;

        // size of the last block. Strict rule: "blockSizeLast <= blockSize"
        uint64_t blockSizeLast;

        // number of BunyArBlockPointer
        uint64_t blockCount;

        // blocks are written after header
        // BunyArBlockPointer blockPointers[blockCount];
    };

// Block pointer represents format, offset, and size of the block
// 1 + 40 + 23
// 1  bit   boolean set when block is compressed
// 40 bits  block offset, max offset is 1024GB-1B
// 23 bits  block size minus one, max size is 8MB
#define BUNYAR_BLOCK_MAX_SIZE_MINUS_ONE (((uint32_t)1 << 23) - 1)
#define BUNYAR_BLOCK_MAX_OFFSET         (((uint64_t)1 << 40) - 1)
    typedef uint64_t BunyArBlockPointer;

    struct BunyArBlockInfo
    {
        bool     isCompressed;
        // size of compressed block in archive
        // uncompressed size is equal to blockSize or blockSizeLast
        uint32_t size;
        // block offset relative to the end of blockPointers
        uint64_t offset;
    };

    static inline struct BunyArBlockInfo bunyArDecodeBlockPointer(BunyArBlockPointer ptr)
    {
        struct BunyArBlockInfo info;

        info.isCompressed = (bool)(ptr & 1);
        info.size = ((uint32_t)(ptr >> 41) & BUNYAR_BLOCK_MAX_SIZE_MINUS_ONE);
        info.offset = ((ptr >> 1) & BUNYAR_BLOCK_MAX_OFFSET);

        info.size += 1;
        return info;
    }

    static inline bool bunyArEncodeBlockPointer(struct BunyArBlockInfo info, BunyArBlockPointer* dst)
    {
        info.size -= 1;

        if (info.offset > BUNYAR_BLOCK_MAX_OFFSET || info.size > BUNYAR_BLOCK_MAX_SIZE_MINUS_ONE)
            return false;

        *dst = ((uint64_t)info.isCompressed) | ((uint64_t)info.offset << 1) | ((uint64_t)info.size << 41);
        return true;
    }

    struct BunyArHashTable
    {
        uint64_t reserved; // 0 "magic" for later
        uint64_t seed;
        uint64_t tableSlotCount;

        // table is located after header
        // uint64_t table[tableSlotCount];
    };

    // user must deallocate returned pointer using tf_free
    FORGE_API struct BunyArHashTable* bunyArHashTableConstruct(uint64_t nodeCount, const struct BunyArNode* nodes, const char* nodeNames);

    // In case value >= nodeCount is returned, node by that name is not found
    FORGE_API uint64_t bunyArHashTableLookup(const struct BunyArHashTable* ht, const char* name, uint64_t nodeCount,
                                             const struct BunyArNode* nodes, const char* nodeNames);

    static inline uint64_t bunyArHashTableSize(const struct BunyArHashTable* ht) { return ht ? ht->tableSlotCount * 8 + sizeof(*ht) : 0; }

    /************************************************************************/
    // MARK: - Advanced Buny Archive file system IO
    /************************************************************************/

    struct BunyArDescription
    {
        uint64_t                      nodeCount;
        const struct BunyArHashTable* hashTable;
    };

    struct BunyArNodeDescription
    {
        const char*           name;
        uint64_t              fileSize;
        uint64_t              compressedSize;
        enum BunyArFileFormat format;
    };

    FORGE_API const char* bunyArFormatName(enum BunyArFileFormat format);

    FORGE_API void fsArchiveGetDescription(IFileSystem* pArchive, struct BunyArDescription* outInfo);

    FORGE_API bool fsArchiveGetNodeDescription(IFileSystem* pArchive, uint64_t nodeId, struct BunyArNodeDescription* outInfo);

    // Same as GetFileUid(), but without fileName postprocessing.
    // Uses fileName directly without resolving through ResourceDirectory
    // to search for file node.
    FORGE_API bool fsArchiveGetNodeId(IFileSystem* fs, const char* fileName, uint64_t* outUid);

    FORGE_API bool fsArchiveGetFileBlockMetadata(FileStream* pFile, struct BunyArBlockFormatHeader* outHeader,
                                                 const BunyArBlockPointer** outBlockPtrs);

    /************************************************************************/
    /************************************************************************/

#ifdef __cplusplus
} // extern "C"
#endif
