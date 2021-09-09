/************************************************************************************

Filename    :   OVR_Stream_Impl.h
Content     :   Implementations of file streams classes.
Created     :   July 2, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include <stdio.h>

#include <vector>
#include <atomic>

#include "OVR_Types.h"
#include "OVR_Stream.h"
//#include "OVR_FileSys.h"

namespace OVRFW {
//==============================================================
// ovrUriScheme
//
// Uses the non-public virtual interface idiom so that the base class
// can do pre- and post- operations when calling overloaded methods on
// derived classes.
class ovrUriScheme {
   public:
    ovrUriScheme(char const* schemeName);
    virtual ~ovrUriScheme();

    char const* GetSchemeName() const;

    ovrStream* AllocStream() const;
    bool OpenHost(char const* hostName, char const* sourceUri);
    void CloseHost(char const* hostName);

    void Shutdown();

    void StreamOpened(ovrStream& stream) const;
    void StreamClosed(ovrStream& stream) const;

    bool HostExists(char const* hostName) const;

   private:
    char SchemeName[ovrFileSys::OVR_MAX_SCHEME_LEN];
    std::string Uri;

    mutable std::atomic<int>
        NumOpenStreams; // this is used to catch the case where a stream is open when we shutdown.

   private:
    virtual ovrStream* AllocStream_Internal() const = 0;
    virtual bool OpenHost_Internal(char const* hostName, char const* sourceUri) = 0;
    virtual void CloseHost_Internal(char const* hostName) = 0;
    virtual void Shutdown_Internal() = 0;
    virtual void StreamOpened_Internal(ovrStream& stream) const {}
    virtual void StreamClosed_Internal(ovrStream& stream) const {}
    virtual bool HostExists_Internal(char const* hostName) const = 0;
};

//==============================================================
// ovrUriScheme_File
class ovrUriScheme_File : public ovrUriScheme {
   public:
    ovrUriScheme_File(char const* schemeName);

    void AddHostSourceUri(char const* hostName, char const* sourceUri);

    class ovrFileHost {
       public:
        ovrFileHost() : HostName("") {}

        ovrFileHost(char const* hostName, char const* sourceUri) : HostName(hostName) {
            SourceUris.push_back(std::string(sourceUri));
        }

        ovrFileHost(ovrFileHost& other) : HostName(other.HostName) {}

        ~ovrFileHost() {}

        ovrFileHost& operator=(ovrFileHost& rhs) {
            if (this != &rhs) {
                this->HostName = rhs.HostName;
                this->SourceUris = rhs.SourceUris;
                rhs.HostName = "";
                rhs.SourceUris.clear();
            }
            return *this;
        }

        bool Open();
        void Close();

        char const* GetHostName() const {
            return HostName.c_str();
        }
        char const* GetSourceUri(int const index) const {
            return SourceUris[index].c_str();
        }
        int GetNumSourceUris() const {
            return static_cast<int>(SourceUris.size());
        }
        void AddSourceUri(char const* sourceUri);

       private:
        std::string HostName; // localhost or machine name on Windows
        std::vector<std::string>
            SourceUris; // all the base paths for files loaded through this host
    };

    int FindHostIndexByHostName(char const* hostName) const;
    ovrFileHost* FindHostByHostName(char const* hostName) const;

   private:
    std::vector<ovrFileHost*> Hosts;

   private:
    virtual ovrStream* AllocStream_Internal() const OVR_OVERRIDE;
    virtual bool OpenHost_Internal(char const* hostName, char const* sourceUri) OVR_OVERRIDE;
    virtual void CloseHost_Internal(char const* hostName) OVR_OVERRIDE;
    virtual void Shutdown_Internal() OVR_OVERRIDE;
    virtual bool HostExists_Internal(char const* hostName) const OVR_OVERRIDE;
};

//==============================================================
// ovrUriScheme_Apk
class ovrUriScheme_Apk : public ovrUriScheme {
   public:
    ovrUriScheme_Apk(char const* schemeName);
    virtual ~ovrUriScheme_Apk();

    void* GetZipFileForHostName(char const* hostName) const;

   private:
    class ovrApkHost {
       public:
        ovrApkHost() : HostName(""), SourceUri(""), ZipFile(NULL) {}

        ovrApkHost(char const* hostName, char const* sourceUri)
            : HostName(hostName), SourceUri(sourceUri), ZipFile(NULL) {}

        ovrApkHost(ovrApkHost& other)
            : HostName(other.HostName), SourceUri(other.SourceUri), ZipFile(other.ZipFile) {
            // leave host and package names for debugging if we try to use a copied host.
            // other.HostName = "";
            // other.SourceUri = "";
            other.ZipFile = NULL;
        }

        ~ovrApkHost() {
            OVR_ASSERT(ZipFile == NULL); // if this hits the host hasn't been closed on delete
        }

        ovrApkHost& operator=(ovrApkHost& rhs) {
            if (this != &rhs) {
                this->HostName = rhs.HostName;
                this->SourceUri = rhs.SourceUri;
                this->ZipFile = rhs.ZipFile;
                rhs.HostName = "";
                rhs.SourceUri = "";
                rhs.ZipFile = NULL;
            }
            return *this;
        }

        bool Open();
        void Close();

        char const* GetHostName() const {
            return HostName.c_str();
        }
        char const* GetSourceUri() const {
            return SourceUri.c_str();
        }
        void* GetZipFile() const {
            return ZipFile;
        }

       private:
        std::string HostName; // com.oculus.appname
        std::string SourceUri; // file:///data/app/com.oculus.appname-1.apk
        void* ZipFile; // pointer to the open apk file
    };

    std::vector<ovrApkHost*> Hosts;

   private:
    virtual ovrStream* AllocStream_Internal() const OVR_OVERRIDE;
    virtual bool OpenHost_Internal(char const* hostName, char const* sourceUri) OVR_OVERRIDE;
    virtual void CloseHost_Internal(char const* hostName) OVR_OVERRIDE;
    virtual void Shutdown_Internal() OVR_OVERRIDE;
    virtual bool HostExists_Internal(char const* hostName) const OVR_OVERRIDE;

    int FindHostIndexByHostName(char const* hostName) const;
    ovrApkHost* FindHostByHostName(char const* hostName) const;
};

//==============================================================
// ovrStream_File
class ovrStream_File : public ovrStream {
   public:
    ovrStream_File(ovrUriScheme const& scheme);
    virtual ~ovrStream_File();

   private:
    FILE* F;
    std::string Uri;

   private:
    virtual bool GetLocalPathFromUri_Internal(const char* uri, std::string& outputPath)
        OVR_OVERRIDE;
    virtual bool Open_Internal(char const* uri, ovrStreamMode const mode) OVR_OVERRIDE;
    virtual void Close_Internal() OVR_OVERRIDE;
    virtual void Flush_Internal() OVR_OVERRIDE;
    virtual bool Read_Internal(
        std::vector<uint8_t>& outBuffer,
        size_t const bytesToRead,
        size_t& outBytesRead) OVR_OVERRIDE;
    virtual bool ReadFile_Internal(std::vector<uint8_t>& outBuffer) OVR_OVERRIDE;
    virtual bool Write_Internal(void const* inBuffer, size_t const bytesToWrite) OVR_OVERRIDE;
    virtual size_t Tell_Internal() const OVR_OVERRIDE;
    virtual size_t Length_Internal() const OVR_OVERRIDE;
    virtual bool AtEnd_Internal() const OVR_OVERRIDE;

    ovrUriScheme_File const& GetFileScheme() const {
        return *static_cast<ovrUriScheme_File const*>(&GetScheme());
    }
};

//==============================================================
// ovrStream_Apk
class ovrStream_Apk : public ovrStream {
   public:
    ovrStream_Apk(ovrUriScheme const& scheme);
    virtual ~ovrStream_Apk();

   private:
    std::string HostName;
    bool IsOpen;

   private:
    virtual bool GetLocalPathFromUri_Internal(const char* uri, std::string& outputPath)
        OVR_OVERRIDE;
    virtual bool Open_Internal(char const* uri, ovrStreamMode const mode) OVR_OVERRIDE;
    virtual void Close_Internal() OVR_OVERRIDE;
    virtual void Flush_Internal() OVR_OVERRIDE;
    virtual bool Read_Internal(
        std::vector<uint8_t>& outBuffer,
        size_t const bytesToRead,
        size_t& outBytesRead) OVR_OVERRIDE;
    virtual bool ReadFile_Internal(std::vector<uint8_t>& outBuffer) OVR_OVERRIDE;
    virtual bool Write_Internal(void const* inBuffer, size_t const bytesToWrite) OVR_OVERRIDE;
    virtual size_t Tell_Internal() const OVR_OVERRIDE;
    virtual size_t Length_Internal() const OVR_OVERRIDE;
    virtual bool AtEnd_Internal() const OVR_OVERRIDE;

    ovrUriScheme_Apk const& GetApkScheme() const {
        return *static_cast<ovrUriScheme_Apk const*>(&GetScheme());
    }
};

} // namespace OVRFW
