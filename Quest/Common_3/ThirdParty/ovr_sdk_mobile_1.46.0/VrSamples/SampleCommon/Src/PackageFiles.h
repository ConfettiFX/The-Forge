/************************************************************************************

Filename    :   PackageFiles.h
Content     :   Read files from the application package zip
Created     :   August 18, 2014
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
#pragma once

#include <vector>

// The application package is the moral equivalent of the filesystem, so
// I don't feel too bad about making it globally accessible, versus requiring
// an App pointer to be handed around to everything that might want to load
// a texture or model.

namespace OVRFW {

//==============================================================
// OvrApkFile
// RAII class for application packages
//==============================================================
class OvrApkFile {
   public:
    OvrApkFile(void* zipFile);
    ~OvrApkFile();

    operator void*() const {
        return ZipFile;
    }
    operator bool() const {
        return ZipFile != 0;
    }

   private:
    void* ZipFile;
};

//--------------------------------------------------------------
// Functions for reading assets from other application packages
//--------------------------------------------------------------

// Call this to open a specific package and use the returned handle in calls to functions for
// loading from other application packages.
void* ovr_OpenOtherApplicationPackage(const char* packageName);

// Call this to close another application package after loading resources from it.
void ovr_CloseOtherApplicationPackage(void*& zipFile);

// These are probably NOT thread safe!
bool ovr_OtherPackageFileExists(void* zipFile, const char* nameInZip);

// Returns NULL buffer if the file is not found.
bool ovr_ReadFileFromOtherApplicationPackage(
    void* zipFile,
    const char* nameInZip,
    int& length,
    void*& buffer);
bool ovr_ReadFileFromOtherApplicationPackage(
    void* zipFile,
    const char* nameInZip,
    std::vector<uint8_t>& buffer);

//--------------------------------------------------------------
// Functions for reading assets from this process's application package
//--------------------------------------------------------------

// returns the zip file for the applications own package
void* ovr_GetApplicationPackageFile();

// Returns something like "/data/data/com.oculus.vrscript/cache/"
// Applications can write files here and expect them to be cleaned up on
// application uninstall, unlike writing to /sdcard.  Also, this is on a
// proper linux filesystem, so exec permissions can be set.
const char* ovr_GetApplicationPackageCachePath();

// App.cpp calls this very shortly after startup.
// If cachePath is not NULL, compressed files that are read will be written
// out to the cachePath with the CRC as the filename so they can be read
// back in much faster.
void ovr_OpenApplicationPackage(const char* packageName, const char* cachePath);

// These are probably NOT thread safe!
bool ovr_PackageFileExists(const char* nameInZip);

// Returns NULL buffer if the file is not found.
bool ovr_ReadFileFromApplicationPackage(const char* nameInZip, int& length, void*& buffer);

// Returns an empty MemBufferFile if the file is not found.
bool ovr_ReadFileFromApplicationPackage(const char* nameInZip, std::vector<uint8_t>& buffer);

} // namespace OVRFW
