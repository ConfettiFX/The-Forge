/************************************************************************************

Filename    :   OVR_FileSys.h
Content     :   Abraction layer for file systems.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#pragma once

#include "OVR_Stream.h"
#include "OVR_BitFlags.h"
#include "OVR_Std.h"

#include <vector>
#include <sys/stat.h>
#include <assert.h>

#include <jni.h>
typedef struct xrJava_ {
    JavaVM* Vm = nullptr; //< Java Virtual Machine
    JNIEnv* Env = nullptr; //< Thread specific environment
    jobject ActivityObject = 0; //< Java activity object
} xrJava;

namespace OVRFW {

enum ovrPermission { PERMISSION_READ, PERMISSION_WRITE, PERMISSION_EXECUTE };
typedef OVR::BitFlagsT<ovrPermission> permissionFlags_t;

enum ovrStorageType {
    // By default data here is private and other apps shouldn't be able to access data from here
    // Path => "/data/data/", in Note 4 this is 24.67GB
    EST_INTERNAL_STORAGE = 0,

    // Also known as emulated internal storage, as this is part of phone memory( that can't be
    // removed ) which is emulated as external storage in Note 4 this is = 24.64GB, with
    // WRITE_EXTERNAL_STORAGE permission can write anywhere in this storage Path =>
    // "/storage/emulated/0" or "/sdcard",
    EST_PRIMARY_EXTERNAL_STORAGE,

    // Path => "/storage/extSdCard"
    // Can only write to app specific folder - /storage/extSdCard/Android/obb/<app>
    EST_SECONDARY_EXTERNAL_STORAGE,

    EST_COUNT
};

enum ovrFolderType {
    // Root folder, for example:
    //		internal 			=> "/data"
    //		primary external 	=> "/storage/emulated/0"
    //		secondary external 	=> "/storage/extSdCard"
    EFT_ROOT = 0,

    // Files folder
    EFT_FILES,

    // Cache folder, data in this folder can be flushed by OS when it needs more memory.
    EFT_CACHE,

    EFT_COUNT
};

//==============================================================
// ovrFileSys
class ovrFileSys {
   public:
    static const int OVR_MAX_SCHEME_LEN = 128;
    static const int OVR_MAX_HOST_NAME_LEN = 256;
    static const int OVR_MAX_PATH_LEN = 1024;
    static const int OVR_MAX_URI_LEN = 1024;

    virtual ~ovrFileSys() {}

    // FIXME: java-specific context should eventually be abstracted
    static ovrFileSys* Create(xrJava const& javaContext);
    static void Destroy(ovrFileSys*& fs);

    // Opens a stream for the specified Uri.
    virtual ovrStream* OpenStream(char const* uri, ovrStreamMode const mode) = 0;
    // Closes the specified stream.
    virtual void CloseStream(ovrStream*& stream) = 0;

    virtual bool ReadFile(char const* uri, std::vector<uint8_t>& outBuffer) = 0;

    virtual bool FileExists(char const* uri) = 0;
    // Gets the local path for the specified URI. File must exist. Returns false if path is not
    // accessible directly by the file system.
    virtual bool GetLocalPathForURI(char const* uri, std::string& outputPath) = 0;

    static void PushBackSearchPathIfValid(
        xrJava const& java,
        ovrStorageType toStorage,
        ovrFolderType toFolder,
        const char* subfolder,
        std::vector<std::string>& searchPaths);
    static bool GetPathIfValidPermission(
        xrJava const& java,
        ovrStorageType toStorage,
        ovrFolderType toFolder,
        const char* subfolder,
        permissionFlags_t permission,
        std::string& outPath);
};

inline std::string ExtractDirectory(const std::string& s) {
    const int l = static_cast<int>(s.length());
    if (l == 0) {
        return std::string("");
    }

    int end;
    if (s[l - 1] == '/') { // directory ends in a slash
        end = l - 1;
    } else {
        for (end = l - 1; end > 0 && s[end] != '/'; end--)
            ;
        if (end == 0) {
            end = l - 1;
        }
    }
    int start;
    for (start = end - 1; start > -1 && s[start] != '/'; start--)
        ;
    start++;

    return std::string(&s[start], end - start);
}

inline bool FileExists(const char* filename) {
    struct stat st;
    int result = stat(filename, &st);
    return result == 0;
}

inline bool GetFullPath(
    const std::vector<std::string>& searchPaths,
    char const* relativePath,
    char* outPath,
    const int outMaxLen) {
    assert(outPath != NULL && outMaxLen >= 1);

    if (FileExists(relativePath)) {
        OVR::OVR_sprintf(outPath, OVR::OVR_strlen(relativePath) + 1, "%s", relativePath);
        return true;
    }

    for (const auto& searchPath : searchPaths) {
        OVR::OVR_sprintf(outPath, outMaxLen, "%s%s", searchPath.c_str(), relativePath);
        if (FileExists(outPath)) {
            return true; // outpath is now set to the full path
        }
    }
    // just return the relative path if we never found the file
    OVR::OVR_sprintf(outPath, outMaxLen, "%s", relativePath);
    return false;
}

inline bool GetFullPath(
    const std::vector<std::string>& searchPaths,
    char const* relativePath,
    std::string& outPath) {
    char largePath[1024];
    bool result = GetFullPath(searchPaths, relativePath, largePath, sizeof(largePath));
    if (result) {
        outPath = largePath;
    }
    return result;
}

inline bool ToRelativePath(
    const std::vector<std::string>& searchPaths,
    char const* fullPath,
    char* outPath,
    const int outMaxLen) {
    // check if the path starts with any of the search paths
    const int n = static_cast<const int>(searchPaths.size());
    for (int i = 0; i < n; ++i) {
        char const* path = searchPaths[i].c_str();
        if (strstr(fullPath, path) == fullPath) {
            size_t len = OVR::OVR_strlen(path);
            OVR::OVR_sprintf(outPath, outMaxLen, "%s", fullPath + len);
            return true;
        }
    }
    OVR::OVR_sprintf(outPath, outMaxLen, "%s", fullPath);
    return false;
}

inline bool ToRelativePath(
    const std::vector<std::string>& searchPaths,
    char const* fullPath,
    std::string& outPath) {
    char largePath[1024];
    bool result = ToRelativePath(searchPaths, fullPath, largePath, sizeof(largePath));
    outPath = largePath;
    return result;
}

bool HasPermission(const char* fileOrDirName, const permissionFlags_t flags);

} // namespace OVRFW
