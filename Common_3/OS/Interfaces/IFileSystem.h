/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#ifndef IFileSystem_h
#define IFileSystem_h

#include "../Interfaces/IOperatingSystem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FileSystem FileSystem;
typedef struct Path Path;
typedef struct FileStream FileStream;
typedef void (*FileDialogCallbackFn)(const Path* path, void* userData);

// A note on memory management for functions defined in this file:
//
// Any function that returns a pointer that does not have 'Get' in its
// name must have its result freed using the appropriate function
// (e.g. fsFreePath, fsFreeFileWatcher, fsFreeFileSystem, fsCloseStream).
//
// For functions whose names contain 'Get', the return value is guaranteed
// to be valid only while its parent object (usually the first argument to the
// function) lives.

// MARK: - Initialization

/// Initializes the FileSystem API
bool fsInitAPI(void);

/// Frees resources associated with the FileSystem API
void fsDeinitAPI(void);

// MARK: - Path Operations

/// Creates a new Path from `absolutePathString`, where `absolutePathString` is a valid path from the root of
/// `fileSystem`.
///
/// - Returns: The newly created path, or NULL if absolutePathString is not a valid path.
Path* fsCreatePath(const FileSystem* fileSystem, const char* absolutePathString);

/// Returns a copy of  `path` for which the caller has ownership.
Path* fsCopyPath(const Path* path);

/// Frees `path`'s memory, invalidating it for any future calls.
void fsFreePath(Path* path);

/// Returns a reference to the FileSystem that `path` references.
FileSystem* fsGetPathFileSystem(const Path* path);

/// Returns the number of UTF-8 codepoints comprising `path`.
size_t fsGetPathLength(const Path* path);

/// Appends `pathComponent` to `basePath`, returning a new Path for which the caller has ownership.
/// `basePath` is assumed to be a directory.
Path* fsAppendPathComponent(const Path* basePath, const char* pathComponent);

/// Appends `newExtension` to `basePath`, returning a new Path for which the caller has ownership.
/// If `basePath` already has an extension, `newExtension` will be appended to the end.
Path* fsAppendPathExtension(const Path* basePath, const char* newExtension);

/// Appends `newExtension` to `basePath`, returning a new Path for which the caller has ownership.
/// If `basePath` already has an extension, its previous extension will be replaced by `newExtension`.
Path* fsReplacePathExtension(const Path* path, const char* newExtension);

typedef struct PathComponent {
    const char* buffer;
    size_t length;
} PathComponent;

/// Splits `path`` path into its components and returns those components by reference.
/// The `buffer` referenced in the out values is guaranteed to live for as long as the Path object lives.
/// Parameters:
/// - directoryName The PathComponent to fill with `path`'s parent directory name, if it exists. May be NULL.
/// - fileName The PathComponent to fill with `path`'s file name. May be NULL.
/// - extension The PathComponent to fill with `path`'s path extension, if it exists. May be NULL. The PathComponent's buffer is guaranteed to either be NULL or a NULL-terminated string.
void fsGetPathComponents(const Path* path, PathComponent* directoryName, PathComponent* fileName, PathComponent* extension);

/// Copies `path`'s parent path, returning a new Path for which the caller has ownership. May return NULL if `path` has no parent.
Path* fsCopyParentPath(const Path* path);

/// Returns `path`'s directory name as a PathComponent. The return value is guaranteed to live for as long as `path` lives.
PathComponent fsGetPathDirectoryName(const Path* path);

/// Returns `path`'s file name as a PathComponent. The return value is guaranteed to live for as long as `path` lives.
PathComponent fsGetPathFileName(const Path* path);

/// Returns `path`'s extension, excluding the '.'. The return value is guaranteed to live for as long as `path` lives.
/// The returned PathComponent's buffer is guaranteed to either be NULL or a NULL-terminated string.
PathComponent fsGetPathExtension(const Path* path);

/// Copies `path`'s lowercased path extension to buffer, writing at most maxLength UTF-8 codepoints.
/// Returns the length of the full extension; if this is larger than maxLength only part of the extension was written.
/// buffer will be null-terminated if there is sufficient space.
size_t fsGetLowercasedPathExtension(const Path* path, char* buffer, size_t maxLength);

/// Returns the native path string representing this path.
/// The path string is guaranteed to live for as long as the Path object lives.
const char* fsGetPathAsNativeString(const Path* path);
                 
/// Returns true if `pathA` and `pathB` point to the same file within the same FileSystem.
bool fsPathsEqual(const Path* pathA, const Path* pathB);

// MARK: - FileSystem-Independent Queries

/// Copies the current working directory path, returning a new Path for which the caller has ownership.
Path* fsCopyWorkingDirectoryPath(void);

/// Copies the path for the directory containing either the executable (on most platforms) or the app bundle (on macOS),
/// returning a new Path for which the caller has ownership.
Path* fsCopyProgramDirectoryPath(void);

/// Copies the path to the currently running executable, returning a new Path for which the caller has ownership.
Path* fsCopyExecutablePath(void);

/// Copies the executable name (excluding its extension) to `buffer`, writing at most `maxLength` UTF-8 codepoints.
/// Returns the length of the full executable name; if this is larger than `maxLength` only part of the executable name was written.
/// buffer will be null-terminated if there is sufficient space.
size_t fsGetExecutableName(char* buffer, size_t maxLength);

/// Copies the path to the preferences directory for `organisation` and `application`, returning a new Path for which the caller has ownership.
/// The path may not exist in its underlying file system; call `fsCreateDirectory` to create it.
Path* fsCopyPreferencesDirectoryPath(const char* organisation, const char* application);

/// Copies the path to the user's documents directory, returning a new Path for which the caller has ownership.
Path* fsCopyUserDocumentsDirectoryPath(void);

/// Copies the preferred path for output log files, returning a new Path for which the caller has ownership.
Path* fsCopyLogFileDirectoryPath(void);

/// Displays an open file dialog for files in `directory`, calling `callback` for the selected file. The selection is filtered to only include files with extensions `fileExtensions` if `fileExtensionCount` is non-zero.
void fsShowOpenFileDialog(const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc, const char** fileExtensions, size_t fileExtensionCount);

/// Displays a save file dialog for files in `directory`, calling `callback` for the selected file. The selection is filtered to only include files with extensions `fileExtensions` if `fileExtensionCount` is non-zero.
void fsShowSaveFileDialog(const char* title, const Path* directory, FileDialogCallbackFn callback, void* userData, const char* fileDesc, const char** fileExtensions, size_t fileExtensionCount);

// MARK: - File and Directory Queries

/// Gets the creation time of the file at `filePath`. Undefined if no file exists at `filePath`.
time_t fsGetCreationTime(const Path* filePath);

/// Gets the time of last access for the file at `filePath`. Undefined if no file exists at `filePath`.
time_t fsGetLastAccessedTime(const Path* filePath);

/// Gets the time of last modification for the file at `filePath`. Undefined if no file exists at `filePath`.
time_t fsGetLastModifiedTime(const Path* filePath);

/// Copies the file at `sourcePath` to `destinationPath`. If a file already exists at `destinationPath`, the file is only copied if
/// `overwritesIfExists` is true. Returns true if the file was successfully copied and false otherwise.
bool fsCopyFile(const Path* sourcePath, const Path* destinationPath, bool overwriteIfExists);

/// Creates a directory at `directoryPath`, recursively creating parent directories if necessary.
/// Returns true if the directory was able to be created.
bool fsCreateDirectory(const Path* directoryPath);

/// Deletes the file at `path`. Returns true if the file was successfully deleted.
bool fsDeleteFile(const Path* path);

/// Returns true if a file (including directories) exists at `path`.
bool fsFileExists(const Path* path);

/// Returns true if a file exists at `path` and that file is a directory.
bool fsDirectoryExists(const Path* path);

/// Enumerates all files with` in `directory`, calling `processFile` for each match.
/// `processFile` should return true if enumerating should continue or false to stop early.
void fsEnumerateFilesInDirectory(const Path* directory, bool (*processFile)(const Path*, void* userData), void* userData);

/// Enumerates all files with the extension `extension` in `directory`, calling `processFile` for each match.
/// `processFile` should return true if enumerating should continue or false to stop early.
void fsEnumerateFilesWithExtension(const Path* directory, const char* extension, bool (*processFile)(const Path*, void* userData), void* userData);

/// Enumerates all subdirectories of `directory`, calling `processDirectory` for each match.
/// `processDirectory` should return true if enumerating should continue or false to stop early.
void fsEnumerateSubDirectories(const Path* directory, bool (*processDirectory)(const Path*, void* userData), void* userData);

// MARK: - Resource Directories

typedef enum ResourceDirectory
{
    /// The main application's shader binaries folder
    RD_SHADER_BINARIES = 0,
    /// The main application's shader source directory
    RD_SHADER_SOURCES,
    /// The main application's texture source directory (TODO processed texture folder)
    RD_TEXTURES,
    RD_MESHES,
    RD_BUILTIN_FONTS,
    RD_GPU_CONFIG,
    RD_ANIMATIONS,
    RD_AUDIO,
    RD_OTHER_FILES,

    // Libraries can have their own directories.
    // Up to 100 libraries are supported.
    ____rd_lib_counter_begin = RD_OTHER_FILES,

    // Add libraries here
    RD_MIDDLEWARE_0,
    RD_MIDDLEWARE_1,
    RD_MIDDLEWARE_2,
    RD_MIDDLEWARE_3,

    ____rd_lib_counter_end = ____rd_lib_counter_begin + 99 * 3,
    RD_ROOT,
    RD_COUNT
} ResourceDirectory;

/// Gets the default relative path for `resourceDir``. For example, given `RD_SHADER_BINARIES`, the return value
/// might be `Shaders/D3D12/Binary` for a DirectX 12 application.
const char* fsGetDefaultRelativePathForResourceDirectory(ResourceDirectory resourceDir);

/// Returns true if resources are bundled together with the application on the target platform. 
bool fsPlatformUsesBundledResources(void);

/// Copies the path to the root ResourceDirectory for `fileSystem`, returning a new Path for which the caller has ownership.
Path* fsCopyResourceDirectoryRootPath(void);

/// Copies the path to `resourceDir` within `fileSystem`, returning a new Path for which the caller has ownership.
Path* fsCopyPathForResourceDirectory(ResourceDirectory resourceDir);

/// Forms a path by appending `relativePath` to the path for `resourceDir` in `fileSystem`, returning a new Path for which the caller has ownership.
Path* fsCopyPathInResourceDirectory(ResourceDirectory resourceDir, const char* relativePath);

/// Returns true if a file exists at `relativePath` within `resourceDir` on `fileSystem`.
bool fsFileExistsInResourceDirectory(ResourceDirectory resourceDir, const char* relativePath);

/// Sets the root resource directory path (which all other resource directory paths are by default relative to) to `path`.
/// Equivalent to calling fsSetPathForResourceDirectory with RD_ROOT.
///
/// NOTE: This call is not thread-safe. It is the application's responsibility to ensure that
/// no modifications to the file system are occurring at the time of this call.
void fsSetResourceDirectoryRootPath(const Path* path);

/// Sets the absolute path for `resourceDir` to `path`. Future modifications through `fsSetResourceDirectoryRootPath`
/// do not affect `resourceDir` after this call.
///
/// NOTE: This call is not thread-safe. It is the application's responsibility to ensure that
/// no modifications to the file system are occurring at the time of this call.
void fsSetPathForResourceDirectory(ResourceDirectory resourceDir, const Path* path);

/// Sets the relative path for `resourceDir` on `fileSystem` to `relativePath`, where the base path is the root resource directory path.
/// If `fsSetResourceDirectoryRootPath` is called after this function, this function must be called again to ensure `resourceDir`'s path
/// is relative to the new root path.
///
/// NOTE: This call is not thread-safe. It is the application's responsibility to ensure that
/// no modifications to the file system are occurring at the time of this call.
void fsSetRelativePathForResourceDirectory(ResourceDirectory resourceDir, const char* relativePath);

/// Resets all resource directories (including the root) on `fileSystem` to their default values.
///
/// NOTE: This call is not thread-safe. It is the application's responsibility to ensure that
/// no modifications to the file system are occurring at the time of this call.
void fsResetResourceDirectories(void);

// MARK: - FileStream

typedef enum SeekBaseOffset
{
    SBO_START_OF_FILE = 0,
    SBO_CURRENT_POSITION,
    SBO_END_OF_FILE,
} SeekBaseOffset;

typedef enum FileMode
{
    FM_READ = 1 << 0,
    FM_WRITE = 1 << 1,
    FM_APPEND = 1 << 2,
    FM_BINARY = 1 << 3,
    FM_READ_WRITE = FM_READ | FM_WRITE,
    FM_READ_APPEND = FM_READ | FM_APPEND,
    FM_WRITE_BINARY = FM_WRITE | FM_BINARY,
    FM_READ_BINARY = FM_READ | FM_BINARY,
    FM_APPEND_BINARY = FM_APPEND | FM_BINARY,
    FM_READ_WRITE_BINARY = FM_READ | FM_WRITE | FM_BINARY,
    FM_READ_APPEND_BINARY = FM_READ | FM_APPEND | FM_BINARY,
} FileMode;

/// Converts `modeStr` to a `FileMode` mask, where `modeStr` follows the C standard library conventions
/// for `fopen` parameter strings.
FileMode fsFileModeFromString(const char* modeStr);

/// Converts `mode` to a string which is compatible with the C standard library conventions for `fopen`
/// parameter strings.
const char* fsFileModeToString(FileMode mode);

/// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
/// to read from or modify the file. May return NULL if the file could not be opened.
FileStream* fsOpenFile(const Path* filePath, FileMode mode);

/// Opens the file at `relativePath` within `resourceDir` using the mode `mode`, returning
/// a new FileStream that can be used to read from or modify the file. May return NULL if the file could not be opened.
FileStream* fsOpenFileInResourceDirectory(ResourceDirectory resourceDir, const char *relativePath, FileMode mode);

/// Opens a FILE* as a FileStream. The caller 
FileStream* fsCreateStreamFromFILE(FILE* file);

/// Opens a read-only buffer as a FileStream, returning a stream that must be closed with `fsCloseStream`.
FileStream* fsOpenReadOnlyMemory(const void *buffer, size_t bufferLengthInBytes);

/// Opens a read-write buffer as a FileStream, returning a stream that must be closed with `fsCloseStream`.
FileStream* fsOpenReadWriteMemory(void *buffer, size_t bufferLengthInBytes);

/// Returns the underlying byte buffer for this stream if present, or NULL if the stream is not backed by a byte buffer.
/// The returned buffer is owned by the stream.
/// It is an error to write to the returned buffer if the stream is read-only.
void* fsGetStreamBufferIfPresent(FileStream* stream);

/// Reads at most `bufferSizeInBytes` bytes from the file and copies them into outputBuffer.
/// Returns the number of bytes read.
size_t fsReadFromStream(FileStream* stream, void* outputBuffer, size_t bufferSizeInBytes);

size_t __fsScanFromStream(FileStream* stream, int* bytesRead, const char* format, ...);

/// Reads formatted data from `stream`, returning the items of the argument list successfully filled.
/// The second argument must be a pointer to an int that will contain the number of bytes read.
/// See `fscanf` in the C standard library for reference.
/// NOTE: this is defined as a macro so we can append %n to the format string, allowing us
/// to get the number of bytes read back from the underlying C function.
#define fsScanFromStream(S, bytesRead, FMT, ...) ( \
    (__fsScanFromStream(S, bytesRead, FMT "%n", __VA_ARGS__, bytesRead)))

/// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
/// Returns the number of bytes written.
size_t fsWriteToStream(FileStream* stream, const void* sourceBuffer, size_t byteCount);

/// Writes the C string pointed by `format` to the stream. If` format` includes format specifiers
/// (subsequences beginning with %), the additional arguments following format are formatted and
/// inserted in the resulting string replacing their respective specifiers.
/// Returns the total number of characters written.
/// See `fprintf` in the C standard library for reference.
size_t fsPrintToStream(FileStream* stream, const char* format, ...);

/// Writes the C string pointed by `format` to the stream. If` format` includes format specifiers
/// (subsequences beginning with %), the additional arguments following format are formatted and
/// inserted in the resulting string replacing their respective specifiers.
/// Returns the total number of characters written.
/// See `vfprintf` in the C standard library for reference.
size_t fsPrintToStreamV(FileStream* stream, const char* format, va_list args);

/// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
bool fsSeekStream(FileStream* stream, SeekBaseOffset baseOffset, ssize_t seekOffset);

/// Gets the current seek position in the file.
ssize_t fsGetStreamSeekPosition(const FileStream* stream);

/// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
ssize_t fsGetStreamFileSize(const FileStream* stream);

/// Flushes all writes to the file stream to the underlying subsystem.
void fsFlushStream(FileStream* stream);

/// Returns whether the current seek position is at the end of the file stream.
bool fsStreamAtEnd(const FileStream* stream);

/// Closes and invalidates the file stream.
bool fsCloseStream(FileStream* stream);

// MARK: FileStream Typed Reads

int64_t         fsReadFromStreamInt64(FileStream* stream);
int32_t         fsReadFromStreamInt32(FileStream* stream);
int16_t         fsReadFromStreamInt16(FileStream* stream);
int8_t          fsReadFromStreamInt8(FileStream* stream);
uint64_t        fsReadFromStreamUInt64(FileStream* stream);
uint32_t        fsReadFromStreamUInt32(FileStream* stream);
uint16_t        fsReadFromStreamUInt16(FileStream* stream);
uint8_t         fsReadFromStreamUInt8(FileStream* stream);
bool            fsReadFromStreamBool(FileStream* stream);
float           fsReadFromStreamFloat(FileStream* stream);
double          fsReadFromStreamDouble(FileStream* stream);

size_t          fsReadFromStreamString(FileStream* stream, char* buffer, size_t maxLength);
size_t          fsReadFromStreamLine(FileStream* stream, char* buffer, size_t maxLength);

// MARK: FileStream Typed Writes

bool            fsWriteToStreamInt64(FileStream* stream, int64_t value);
bool            fsWriteToStreamInt32(FileStream* stream, int32_t value);
bool            fsWriteToStreamInt16(FileStream* stream, int16_t value);
bool            fsWriteToStreamInt8(FileStream* stream, int8_t value);
bool            fsWriteToStreamUInt64(FileStream* stream, uint64_t value);
bool            fsWriteToStreamUInt32(FileStream* stream, uint32_t value);
bool            fsWriteToStreamUInt16(FileStream* stream, uint16_t value);
bool            fsWriteToStreamUInt8(FileStream* stream, uint8_t value);
bool            fsWriteToStreamBool(FileStream* stream, bool value);
bool            fsWriteToStreamFloat(FileStream* stream, float value);
bool            fsWriteToStreamDouble(FileStream* stream, double value);
bool            fsWriteToStreamString(FileStream* stream, const char* value);
bool            fsWriteToStreamLine(FileStream* stream, const char* value);

// MARK: - FileWatcher

typedef struct FileWatcher FileWatcher;

typedef void (*FileWatcherCallback)(const Path* path, uint32_t action);

typedef enum FileWatcherEventMask {
    FWE_MODIFIED = 1 << 0,
    FWE_ACCESSED = 1 << 1,
    FWE_CREATED = 1 << 2,
    FWE_DELETED = 1 << 3,
} FileWatcherEventMask;

/// Creates a new FileWatcher that watches for changes specified by `eventMask` at `path` and calls `callback` when changes occur.
/// The return value must have `fsFreeFileWatcher` called to free it.
FileWatcher* fsCreateFileWatcher(const Path* path, FileWatcherEventMask eventMask, FileWatcherCallback callback);

/// Invalidates and frees `fileWatcher.
void fsFreeFileWatcher(FileWatcher* fileWatcher);

// MARK: - FileSystem

typedef enum FileSystemKind
{
    FSK_SYSTEM = 0,
    FSK_ZIP,
    FSK_RESOURCE_BUNDLE,
    
    FSK_DEFAULT = FSK_SYSTEM
} FileSystemKind;

FileSystem* fsGetSystemFileSystem(void);
FileSystemKind fsGetFileSystemKind(const FileSystem* fileSystem);

typedef enum FileSystemFlags {
    FSF_CREATE_IF_NECESSARY = 1 << 0,
    FSF_OVERWRITE = 1 << 1,
    FSF_READ_ONLY = 1 << 2,
} FileSystemFlags;

/// Creates a new file-system with its root at rootPath.
/// If rootPath is a compressed zip file, the file system will be the contents of the zip file.
FileSystem* fsCreateFileSystemFromFileAtPath(const Path* rootPath, FileSystemFlags flags);

/// If `fileSystem` is parented under another file system (e.g. is a zip file system), returns the path
/// to the file system under its parent file system.
/// Otherwise, returns NULL.
Path* fsCopyPathInParentFileSystem(const FileSystem* fileSystem);

/// Decrements the reference count for `fileSystem`, freeing it if there are no outstanding references.
/// NOTE: `Path`s and `FileStream`s hold references to their FileSystem.
void fsFreeFileSystem(FileSystem* fileSystem);

/// Returns true if the file system is read-only.
bool fsFileSystemIsReadOnly(const FileSystem* fileSystem);

#ifdef __cplusplus
} // extern "C"
#endif

// MARK: - C++ Extras

#ifdef __cplusplus
// These types are only defined in C++, so conditionalise them out unless we're importing this from a C++ source file.

float2          fsReadFromStreamFloat2(FileStream* stream);
float3          fsReadFromStreamFloat3(FileStream* stream);
float3          fsReadFromStreamPackedFloat3(FileStream* stream, float maxAbsCoord);
float4          fsReadFromStreamFloat4(FileStream* stream);

bool            fsWriteToStreamFloat2(FileStream* stream, float2 value);
bool            fsWriteToStreamFloat3(FileStream* stream, float3 value);
bool            fsWriteToStreamPackedFloat3(FileStream* stream, float3 value, float maxAbsCoord);
bool            fsWriteToStreamFloat4(FileStream* stream, float4 value);

template<typename T>
static inline T fsReadFromStreamType(FileStream* stream) {
    T result;
    fsReadFromStream(stream, &result, sizeof(T));
    return result;
}

template<typename T>
static inline bool fsWriteToStreamType(FileStream* stream, T value) {
    return fsWriteToStream(stream, &value, sizeof(T)) == sizeof(T);
}

/// PathHandle is a RAII wrapper for `Path`.
/// Any function that returns a `Path*` may have its result assigned to a `PathHandle`,
/// and that path will automatically be freed when the PathHandle goes out of scope.
class PathHandle {
private:
    Path* pPath;
    
public:
    inline PathHandle() : pPath(nullptr) {}
    
    inline PathHandle(Path *path) : pPath(path) {}
    
    inline PathHandle(const FileSystem* fileSystem, const char* absolutePathString) : pPath(fsCreatePath(fileSystem, absolutePathString)) {}
    
    inline PathHandle(const PathHandle& other) : pPath(fsCopyPath(other.pPath)) {}
    
    inline ~PathHandle() {
        fsFreePath(pPath);
    }
    
    inline operator const Path*() const { return pPath; }
    
    inline PathHandle& operator=( const PathHandle& other ) {
        fsFreePath(pPath);
        pPath = fsCopyPath(other.pPath);
        return *this;
    }
    
    inline operator bool() const {
        return pPath != nullptr;
    }
    
    inline bool operator==(const PathHandle& other) const {
        return fsPathsEqual(pPath, other.pPath);
    }
};

#endif // ifdef __cplusplus

#if TARGET_OS_IPHONE
void fsRegisterUTIForExtension(const char* uti, const char* extension);
#endif

#endif // ifndef IFileSystem_h

// We define a couple of extras for if EASTL's string type is defined.
// This is under a separate include guard in case this header is included transitively,
// EASTL is included, and then this header is included again.

#if defined(EASTL_STRING_H) && !defined(IFileSystem_h_STLString)
#define IFileSystem_h_STLString

eastl::string fsReadFromStreamSTLString(FileStream* stream);
eastl::string fsReadFromStreamSTLLine(FileStream* stream);

/// Forms an `eastl::string` from `pathComponent`
inline eastl::string fsPathComponentToString(PathComponent pathComponent) {
    return eastl::string(pathComponent.buffer, pathComponent.length);
}

/// Returns `path`'s file name and extension as a string. The return value is guaranteed to live for as long as `path` lives.
eastl::string fsGetPathFileNameAndExtension(const Path* path);

#endif // defined(EASTL_STRING_H) && !defined(IFileSystem_h_STLString)

#if defined(EASTL_VECTOR_H) && !defined(IFileSystem_h_STLVector)
#define IFileSystem_h_STLVector

/// Collects the results of calling `fsEnumerateFilesWithExtension` with `directory` and `extension` into an `eastl::vector<PathHandle>`.
eastl::vector<PathHandle> fsGetFilesInDirectory(const Path* directory);

/// Collects the results of calling `fsEnumerateFilesWithExtension` with `directory` and `extension` into an `eastl::vector<PathHandle>`.
eastl::vector<PathHandle> fsGetFilesWithExtension(const Path* directory, const char* extension);

/// Collects the results of calling `fsEnumerateSubdirectories` with `directory` into an `eastl::vector<PathHandle>`.
eastl::vector<PathHandle> fsGetSubDirectories(const Path* directory);

#endif // defined(EASTL_VECTOR_H) && !defined(IFileSystem_h_STLVector)

#if defined(EASTL_FUNCTIONAL_H) && !defined(IFileSystem_h_STLFunctional)
#define IFileSystem_h_STLFunctional

namespace eastl {

  template <>
  struct hash<PathHandle>
  {
    std::size_t inline operator()(const PathHandle& k) const
    {
        const char* s = fsGetPathAsNativeString(k);
        const FileSystem* fs = fsGetPathFileSystem(k);
        
        size_t h = eastl::hash<const FileSystem*>()(fs);
        
        while (*s) {
            h = (h * 54059) ^ (s[0] * 76963);
            s++;
        }
        return h;
    }
  };
}

#endif // defined(EASTL_FUNCTIONAL_H) && !defined(IFileSystem_h_STLFunctional)
