/************************************************************************************

Filename    :   OVR_FileSys.cpp
Content     :   Abraction layer for file systems.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "OVR_FileSys.h"

#include <vector>
#include <cctype> // for isdigit, isalpha

#include "Misc/Log.h"

#include "OVR_Stream_Impl.h"
#include "OVR_UTF8Util.h"
#include "OVR_Uri.h"
#include "OVR_Std.h"
#include "JniUtils.h"

namespace OVRFW {

bool HasPermission(const char* fileOrDirName, const permissionFlags_t flags) {
    OVR_ASSERT(flags.GetValue() != 0);

    std::string s(fileOrDirName);
    int len = static_cast<int>(s.length());
    if (len <= 0)
        return false;

    if (s[len - 1] != '/') { // directory ends in a slash
        int end = len - 1;
        for (; end > 0 && s[end] != '/'; end--)
            ;
        s = std::string(&s[0], end);
    }

    int mode = 0;
    if (flags & PERMISSION_WRITE) {
        mode |= W_OK;
    }
    if (flags & PERMISSION_READ) {
        mode |= R_OK;
    }
    if (flags & PERMISSION_EXECUTE) {
        mode |= X_OK;
    }
    return access(s.c_str(), mode) == 0;
}

static const char* StorageName[EST_COUNT] = {
    "Phone Internal", // "/data/data/"
    "Phone External", // "/storage/emulated/0" or "/sdcard"
    "SD Card External" // "/storage/extSdCard", "", or a dynamic UUID if Android-M
};

static const char* FolderName[EFT_COUNT] = {"Root", "Files", "Cache"};

bool ovrFileSys::GetPathIfValidPermission(
    xrJava const& java,
    ovrStorageType storageType,
    ovrFolderType folderType,
    const char* subfolder,
    permissionFlags_t permission,
    std::string& outPath) {
    std::string storageBasePath = "";
    // Hard-coding these values for now
    if (storageType == EST_SECONDARY_EXTERNAL_STORAGE && folderType == EFT_ROOT) {
        storageBasePath += "/sdcard/";
    } else if (storageType == EST_PRIMARY_EXTERNAL_STORAGE && folderType == EFT_ROOT) {
        storageBasePath += "/sdcard/";
    } else if (storageType == EST_INTERNAL_STORAGE && folderType == EFT_ROOT) {
        storageBasePath += "/data/data/";
    } else {
        // TODO ... figure this out.
        ALOGW(
            "Failed to get permission for %s storage in %s folder ",
            StorageName[storageType],
            FolderName[folderType]);
        return false;
    }
    std::string checkPath = storageBasePath + subfolder;
    return HasPermission(checkPath.c_str(), permission);
}

void ovrFileSys::PushBackSearchPathIfValid(
    xrJava const& java,
    ovrStorageType storageType,
    ovrFolderType folderType,
    const char* subfolder,
    std::vector<std::string>& searchPaths) {
    std::string storageBasePath = "";
    if (storageType == EST_SECONDARY_EXTERNAL_STORAGE && folderType == EFT_ROOT) {
        storageBasePath += "/sdcard/";
    } else if (storageType == EST_PRIMARY_EXTERNAL_STORAGE && folderType == EFT_ROOT) {
        storageBasePath += "/sdcard/";
    } else if (storageType == EST_INTERNAL_STORAGE && folderType == EFT_ROOT) {
        storageBasePath += "/data/data/";
    } else {
        // TODO ... figure this out.
        ALOGW(
            "Failed to get permission for %s storage in %s folder ",
            StorageName[storageType],
            FolderName[folderType]);
        return;
    }
    std::string checkPath = storageBasePath + subfolder;

    if (HasPermission(checkPath.c_str(), permissionFlags_t(PERMISSION_READ))) {
        searchPaths.push_back(checkPath);
    }
}

//==============================================================
// ovrFileSysLocal
class ovrFileSysLocal : public ovrFileSys {
   public:
    // this is yucky right now because it's Java-specific, even though windows doesn't care about
    // it.
    ovrFileSysLocal(xrJava const& javaContext);
    virtual ~ovrFileSysLocal();

    virtual ovrStream* OpenStream(char const* uri, ovrStreamMode const mode);
    virtual void CloseStream(ovrStream*& stream);
    virtual bool ReadFile(char const* uri, std::vector<uint8_t>& outBuffer);
    virtual bool FileExists(char const* uri);
    virtual bool GetLocalPathForURI(char const* uri, std::string& outputPath);

    virtual void Shutdown();

   private:
    std::vector<ovrUriScheme*> Schemes;
    JavaVM* Jvm{nullptr};
    jobject ActivityObject{0};

   private:
    int FindSchemeIndexForName(char const* schemeName) const;
    ovrUriScheme* FindSchemeForName(char const* name) const;
};

#define PUI_PACKAGE_NAME "com.oculus.systemactivities"

//==============================
// ovrFileSysLocal::ovrFileSysLocal
ovrFileSysLocal::ovrFileSysLocal(xrJava const& javaContext) : Jvm(javaContext.Vm) {
    // always do unit tests on startup to assure nothing has been broken
    ovrUri::DoUnitTest();

    ActivityObject = javaContext.Env->NewGlobalRef(javaContext.ActivityObject);

    // add the apk scheme
    ovrUriScheme_Apk* scheme = new ovrUriScheme_Apk("apk");

    // add a host for the executing application's scheme
    char curPackageName[OVR_MAX_PATH_LEN];
    ovr_GetCurrentPackageName(
        javaContext.Env, javaContext.ActivityObject, curPackageName, sizeof(curPackageName));

    char curPackageCodePath[OVR_MAX_PATH_LEN];
    ovr_GetPackageCodePath(
        javaContext.Env,
        javaContext.ActivityObject,
        curPackageCodePath,
        sizeof(curPackageCodePath));

    char curPackageUri[OVR_MAX_URI_LEN];
    OVR::OVR_sprintf(curPackageUri, sizeof(curPackageUri), "file://%s", curPackageCodePath);
    if (!scheme->OpenHost("localhost", curPackageUri)) {
        ALOG("Failed to OpenHost for host '%s', uri '%s'", "localhost", curPackageUri);
        assert(false);
    }

    {
        for (int i = 0; i < 2; ++i) {
            char const* packageName = (i == 0) ? PUI_PACKAGE_NAME : curPackageName;
            char packagePath[OVR_MAX_PATH_LEN];
            packagePath[0] = '\0';
            if (ovr_GetInstalledPackagePath(
                    javaContext.Env,
                    javaContext.ActivityObject,
                    packageName,
                    packagePath,
                    sizeof(packagePath))) {
                char packageUri[sizeof(packagePath) + 7];
                OVR::OVR_sprintf(packageUri, sizeof(packageUri), "file://%s", packagePath);
                ALOG("ovrFileSysLocal - scheme adding name='%s' uri='%s'", packageName, packageUri);
                scheme->OpenHost(packageName, packageUri);
                break;
            }
        }
    }

    // add the host for font assets by opening a stream and trying to load res/raw/font_location.txt
    // from the System Activites apk. If this file exists then
    {
        std::vector<uint8_t> buffer;
        char fileName[256];
        OVR::OVR_sprintf(
            fileName, sizeof(fileName), "apk://%s/res/raw/font_location.txt", PUI_PACKAGE_NAME);
        char fontPackageName[1024];
        bool success = ReadFile(fileName, buffer);
        if (success && buffer.size() > 0) {
            OVR::OVR_strncpy(
                fontPackageName,
                sizeof(fontPackageName),
                (char const*)(static_cast<uint8_t const*>(buffer.data())),
                buffer.size());
            ALOG("Found font package name '%s'", fontPackageName);
        } else {
            // default to the SystemActivities apk.
            OVR::OVR_strcpy(fontPackageName, sizeof(fontPackageName), PUI_PACKAGE_NAME);
        }

        char packagePath[OVR_MAX_PATH_LEN];
        packagePath[0] = '\0';
        if (ovr_GetInstalledPackagePath(
                javaContext.Env,
                javaContext.ActivityObject,
                fontPackageName,
                packagePath,
                sizeof(packagePath))) {
            // add this package to our scheme as a host so that fonts can be loaded from it
            char packageUri[sizeof(packagePath) + 7];
            OVR::OVR_sprintf(packageUri, sizeof(packageUri), "file://%s", packagePath);

            // add the package name as an explict host if it doesn't already exists -- it will
            // already exist if the package name is not overrloaded by font_location.txt (i.e. the
            // fontPackageName will have defaulted to PUI_PACKAGE_NAME )
            if (!scheme->HostExists(fontPackageName)) {
                scheme->OpenHost(fontPackageName, packageUri);
            }
            scheme->OpenHost("font", packageUri);

            ALOG("ovrFileSysLocal - Added host '%s' for fonts @'%s'", fontPackageName, packageUri);
        }
    }

    ALOG("ovrFileSysLocal - apk scheme OpenHost done uri '%s'", curPackageUri);
    Schemes.push_back(scheme);

    ovrUriScheme_File* file_scheme = new ovrUriScheme_File("file");
    if (!file_scheme->OpenHost("localhost", "")) {
        ALOG("Failed to OpenHost for file_scheme host '%s', uri '%s'", "localhost", "");
        assert(false);
    }
    ALOG("ovrFileSysLocal - file scheme OpenHost done uri '%s'", "");
    Schemes.push_back(file_scheme);

    ALOG("ovrFileSysLocal - done ");
}

//==============================
// ovrFileSysLocal::ovrFileSysLocal
ovrFileSysLocal::~ovrFileSysLocal() {
#if defined(OVR_OS_ANDROID)
    TempJniEnv env{Jvm};
    env->DeleteGlobalRef(ActivityObject);
#endif
}

//==============================
// ovrFileSysLocal::OpenStream
ovrStream* ovrFileSysLocal::OpenStream(char const* uri, ovrStreamMode const mode) {
    // parse the Uri to find the scheme
    char scheme[OVR_MAX_SCHEME_LEN];
    char host[OVR_MAX_HOST_NAME_LEN];
    char path[OVR_MAX_PATH_LEN];
    int port = 0;
    ovrUri::ParseUri(
        uri,
        scheme,
        sizeof(scheme),
        nullptr,
        0,
        nullptr,
        0,
        host,
        sizeof(host),
        port,
        path,
        sizeof(path),
        nullptr,
        0,
        nullptr,
        0);

    // ALOG( "Uri='%s' scheme='%s' host='%s'", uri, scheme, host );

    ovrUriScheme* uriScheme = FindSchemeForName(scheme);
    if (uriScheme == nullptr) {
        ALOG("Uri '%s' missing scheme! Assuming apk scheme!", uri);
        uriScheme = FindSchemeForName("apk");
        if (uriScheme == nullptr) {
            return nullptr;
        }
    }

#if defined(OVR_OS_ANDROID)
    // If apk scheme, need to check if this is a package we haven't seen before, and add a host if
    // so.
    if (OVR::OVR_stricmp(scheme, "apk") == 0) {
        if (!uriScheme->HostExists(host)) {
            TempJniEnv env{Jvm};
            char packagePath[OVR_MAX_PATH_LEN];
            packagePath[0] = '\0';
            if (ovr_GetInstalledPackagePath(
                    env, ActivityObject, host, packagePath, sizeof(packagePath))) {
                char packageUri[sizeof(packagePath) + 7];
                OVR::OVR_sprintf(packageUri, sizeof(packageUri), "file://%s", packagePath);
                uriScheme->OpenHost(host, packageUri);
            }
        }
    }
#endif

    ovrStream* stream = uriScheme->AllocStream();
    if (stream == nullptr) {
        // ALOG( "Uri='%s' AllocStream failed!", uri );
        assert(stream != nullptr);
        return nullptr;
    }
    if (!stream->Open(uri, mode)) {
        // ALOG( "Uri='%s' stream->Open failed!", uri );
        delete stream;
        return nullptr;
    }
    return stream;
}

//==============================
// ovrFileSysLocal::CloseStream
void ovrFileSysLocal::CloseStream(ovrStream*& stream) {
    if (stream != nullptr) {
        stream->Close();
        delete stream;
        stream = nullptr;
    }
}

//==============================
// ovrFileSysLocal::ReadFile
bool ovrFileSysLocal::ReadFile(char const* uri, std::vector<uint8_t>& outBuffer) {
    ovrStream* stream = OpenStream(uri, OVR_STREAM_MODE_READ);
    if (stream == nullptr) {
        return false;
    }
    bool success = stream->ReadFile(uri, outBuffer);
    CloseStream(stream);
    return success;
}

//==============================
// ovrFileSysLocal::FileExists
bool ovrFileSysLocal::FileExists(char const* uri) {
    ovrStream* stream = OpenStream(uri, OVR_STREAM_MODE_READ);
    if (stream == nullptr) {
        return false;
    }
    CloseStream(stream);
    return true;
}

//==============================
// ovrFileSysLocal::GetLocalPathForURI
bool ovrFileSysLocal::GetLocalPathForURI(char const* uri, std::string& outputPath) {
    // parse the Uri to find the scheme
    char scheme[OVR_MAX_SCHEME_LEN];
    char host[OVR_MAX_HOST_NAME_LEN];
    char path[OVR_MAX_PATH_LEN];
    int port = 0;
    ovrUri::ParseUri(
        uri,
        scheme,
        sizeof(scheme),
        nullptr,
        0,
        nullptr,
        0,
        host,
        sizeof(host),
        port,
        path,
        sizeof(path),
        nullptr,
        0,
        nullptr,
        0);

    ovrUriScheme* uriScheme = FindSchemeForName(scheme);
    if (uriScheme == nullptr) {
        ALOG("GetLocalPathForURI: Uri '%s' missing scheme!", uri);
        return false;
    }

    // FIXME: It would be better to not have to allocate a stream to just get the path
    ovrStream* stream = uriScheme->AllocStream();
    if (stream == nullptr) {
        assert(stream != nullptr);
        return false;
    }

    const bool result = stream->GetLocalPathFromUri(uri, outputPath);
    delete stream;

    return result;
}

//==============================
// ovrFileSysLocal::FindSchemeIndexForName
int ovrFileSysLocal::FindSchemeIndexForName(char const* schemeName) const {
    for (int i = 0; i < static_cast<const int>(Schemes.size()); ++i) {
        if (OVR::OVR_stricmp(Schemes[i]->GetSchemeName(), schemeName) == 0) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrFileSysLocal::FindSchemeForName
ovrUriScheme* ovrFileSysLocal::FindSchemeForName(char const* name) const {
    int index = FindSchemeIndexForName(name);
    return index < 0 ? nullptr : Schemes[index];
}

//==============================
// ovrFileSysLocal::Shutdown
void ovrFileSysLocal::Shutdown() {
    for (int i = 0; i < static_cast<const int>(Schemes.size()); ++i) {
        Schemes[i]->Shutdown();
        delete Schemes[i];
        Schemes[i] = nullptr;
    }
    Schemes.clear();
}

//==============================================================================================
// ovrFileSys
//==============================================================================================

//==============================
// ovrFileSys::Create
ovrFileSys* ovrFileSys::Create(xrJava const& javaContext) {
    ovrFileSys* fs = new ovrFileSysLocal(javaContext);
    return fs;
}

//==============================
// ovrFileSys::Destroy
void ovrFileSys::Destroy(ovrFileSys*& fs) {
    if (fs != nullptr) {
        ovrFileSysLocal* fsl = static_cast<ovrFileSysLocal*>(fs);
        fsl->Shutdown();
        delete fs;
        fs = nullptr;
    }
}

} // namespace OVRFW
