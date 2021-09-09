/************************************************************************************

Filename    :   OVR_Stream.cpp
Content     :   Abstraction for file streams.
Created     :   July 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#include "OVR_Stream.h"

#include "Misc/Log.h"

#include <stdio.h>

#include "PackageFiles.h"
#include "OVR_Uri.h"
#include "OVR_UTF8Util.h"
#include "OVR_FileSys.h"
#include "OVR_Stream_Impl.h"

#include "OVR_Std.h"

namespace OVRFW {

bool UriPathStartsWithDriveLetter(char const* uriPath) {
    if (uriPath == NULL || uriPath[0] == '\0') {
        return false;
    }
    if (uriPath[0] != '/') {
        return false;
    }
    size_t len = UTF8Util::GetLength(uriPath);
    if (len < 3) {
        return false;
    }
    if (uriPath[0] != '/' || uriPath[2] != ':') {
        return false;
    }
    return true;
}

// this takes a pointer to a buffer containing a URI path as input and outputs a path that is safe
// to use on Windows. In particular, if the path starts with the form "/C:", then the returned path
// skips past the '/' and starts with the drive letter.
const char* SafePathFromUriPath(char const* uriPath) {
    if (UriPathStartsWithDriveLetter(uriPath)) {
        return uriPath + 1;
    }
    return uriPath;
}

static uint32_t DecodeNextChar(char const* p, intptr_t& offset) {
    char const* t = p + offset;
    uint32_t ch = UTF8Util::DecodeNextChar(&t);
    offset = t - p;
    return ch;
}

static bool EncodeChar(char* p, size_t const& maxOffset, intptr_t& offset, uint32_t ch) {
    // test for buffer overflow by encoding to a temp buffer and seeing how far the offset moved
    char temp[6];
    intptr_t tempOfs = 0;
    UTF8Util::EncodeChar(temp, &tempOfs, ch);
    if (static_cast<intptr_t>(maxOffset) - offset <= tempOfs) {
        // just encode a null byte at the current offset
        assert(false);
        p[offset] = '\0';
        offset++;
        return false;
    }
    UTF8Util::EncodeChar(p, &offset, ch);
    return true;
}

bool AppendUriPath(
    char const* inPath,
    char const* appendPath,
    char* outPath,
    size_t const outPathSize) {
    const char WIN_PATH_SEPARATOR = '\\';
    const char NIX_PATH_SEPARATOR = '/';
    const char URI_PATH_SEPARATOR = '/';

    if (inPath == NULL || outPath == NULL || appendPath == NULL || outPathSize < 2) {
        assert(inPath != NULL && outPath != NULL && appendPath != NULL && outPathSize > 1);
        return false;
    }
    intptr_t inOfs = 0;
    intptr_t outOfs = 0;
    uint32_t lastCh = 0xffffffff;
    uint32_t ch = 0xffffffff;
    for (;;) {
        lastCh = ch;
        ch = DecodeNextChar(inPath, inOfs);
        if (ch == '\0') {
            break;
        }
        if (!EncodeChar(outPath, outPathSize, outOfs, ch)) {
            return false;
        }
    }

    // ensure there's always a path separator after inPath
    if (lastCh != WIN_PATH_SEPARATOR && lastCh != NIX_PATH_SEPARATOR &&
        lastCh != URI_PATH_SEPARATOR) {
        // emit a separator
        if (!EncodeChar(outPath, outPathSize, outOfs, URI_PATH_SEPARATOR)) {
            return false; // buffer overflow
        }
    }

    // skip past any path separators at the start of append path
    intptr_t appendOfs = 0;
    char const* appendPathStart = &appendPath[0];
    for (;;) {
        ch = DecodeNextChar(appendPath, appendOfs);
        if (ch != WIN_PATH_SEPARATOR && ch != NIX_PATH_SEPARATOR && ch != URI_PATH_SEPARATOR) {
            break;
        }
        appendPathStart = appendPath + appendOfs;
    }

    appendOfs = 0;
    for (;;) {
        ch = DecodeNextChar(appendPathStart, appendOfs);
        if (!EncodeChar(outPath, outPathSize, outOfs, ch)) {
            return false;
        }
        if (ch == 0) {
            return true;
        }
    }
}

//==============================================================================================
// ovrUriScheme
//==============================================================================================

//==============================
// ovrUriScheme::ovrUriScheme
ovrUriScheme::ovrUriScheme(char const* schemeName) : SchemeName(), Uri(""), NumOpenStreams(0) {
    OVR::OVR_strcpy(SchemeName, sizeof(SchemeName), schemeName);
}

//==============================
// ovrUriScheme::~ovrUriScheme
ovrUriScheme::~ovrUriScheme() {}

//==============================
// ovrUriScheme::GetSchemeName
char const* ovrUriScheme::GetSchemeName() const {
    return SchemeName;
}

//==============================
// ovrUriScheme::AllocStream
ovrStream* ovrUriScheme::AllocStream() const {
    return AllocStream_Internal();
}

//==============================
// ovrUriScheme::Open
bool ovrUriScheme::OpenHost(char const* hostName, char const* sourceUri) {
    assert(ovrUri::IsValidUri(sourceUri));
    if (!OpenHost_Internal(hostName, sourceUri)) {
        ALOG("Failed to OpenHost for host '%s', uri '%s'", hostName, sourceUri);
        assert(false);
        return false;
    }
    return true;
}

//==============================
// ovrUriScheme::CloseHost
void ovrUriScheme::CloseHost(char const* hostName) {
    CloseHost_Internal(hostName);
}

//==============================
// ovrUriScheme::Shutdown
void ovrUriScheme::Shutdown() {
    assert(NumOpenStreams == 0); // this should never happen -- CLOSE ALL STREAMS AFTER USE.
    Shutdown_Internal();
}

//==============================
// ovrUriScheme::StreamOpened
void ovrUriScheme::StreamOpened(ovrStream& stream) const {
    NumOpenStreams++;
    StreamOpened_Internal(stream);
}

//==============================
// ovrUriScheme::StreamClosed
void ovrUriScheme::StreamClosed(ovrStream& stream) const {
    StreamClosed_Internal(stream);
    NumOpenStreams--;
    assert(NumOpenStreams >= 0); // if this goes negative a stream was closed twice
}

//==============================
// ovrUriScheme::HostExists
bool ovrUriScheme::HostExists(char const* hostName) const {
    return HostExists_Internal(hostName);
}

//==============================================================================================
// ovrStream
//==============================================================================================

//==============================
// ovrStream::ovrStream
ovrStream::ovrStream(ovrUriScheme const& scheme)
    : Scheme(scheme), Uri(""), Mode(OVR_STREAM_MODE_MAX) {}

//==============================
// ovrStream::~ovrStream
ovrStream::~ovrStream() {
    assert(Mode == OVR_STREAM_MODE_MAX); // if this hits, file was not closed first
}

//==============================
// ovrStream::Open
bool ovrStream::Open(char const* uri, ovrStreamMode const mode) {
    if (IsOpen()) {
        ALOG(
            "ovrStream::Open: tried to open Uri '%s' when Uri '%s' is already open",
            uri,
            Uri.c_str());
        assert(!IsOpen());
        return false;
    }

    bool success = Open_Internal(uri, mode);
    if (success) {
        Uri = uri;
        Mode = mode;
        Scheme.StreamOpened(*this);
    }
    return success;
}

//==============================
// ovrStream::Close
void ovrStream::Close() {
    Close_Internal();
    Scheme.StreamClosed(*this);
    Mode = OVR_STREAM_MODE_MAX;
}

//==============================
// ovrStream::Flush
void ovrStream::Flush() {
    Flush_Internal();
}

//==============================
// ovrStream::GetLocalPathFromUri
bool ovrStream::GetLocalPathFromUri(const char* uri, std::string& outputPath) {
    return GetLocalPathFromUri_Internal(uri, outputPath);
}

//==============================
// ovrStream::Read
bool ovrStream::Read(
    std::vector<uint8_t>& outBuffer,
    size_t const bytesToRead,
    size_t& outBytesRead) {
    if (!IsOpen()) {
        ALOG("ovrStream::Read: stream is not open!");
        assert(IsOpen());
        return false;
    }

    if (Mode != OVR_STREAM_MODE_READ) {
        ALOG("ovrStream::Read: stream is not open for reading!");
        assert(Mode == OVR_STREAM_MODE_READ);
        return false;
    }
    return Read_Internal(outBuffer, bytesToRead, outBytesRead);
}

//==============================
// ovrStream::ReadFile
bool ovrStream::ReadFile(char const* uri, std::vector<uint8_t>& outBuffer) {
    assert(IsOpen());

    return ReadFile_Internal(outBuffer);
}

//==============================
// ovrStream::Write
bool ovrStream::Write(void const* inBuffer, size_t const bytesToWrite) {
    if (!IsOpen()) {
        ALOG("ovrStream::Read: stream is not open!");
        assert(IsOpen());
        return false;
    }

    if (Mode != OVR_STREAM_MODE_WRITE) {
        ALOG("ovrStream::Read: stream is not open for writing!");
        assert(Mode == OVR_STREAM_MODE_WRITE);
        return false;
    }
    return Write_Internal(inBuffer, bytesToWrite);
}

//==============================
// ovrStream::Tell
size_t ovrStream::Tell() const {
    return Tell_Internal();
}

//==============================
// ovrStream::Length
size_t ovrStream::Length() const {
    return Length_Internal();
}

//==============================
// ovrStream::GetUri
char const* ovrStream::GetUri() const {
    return Uri.c_str();
}

//==============================
// ovrStream::IsOpen
bool ovrStream::IsOpen() const {
    return Mode != OVR_STREAM_MODE_MAX;
}

//==============================================================================================
// ovrUriScheme_File
//==============================================================================================

//==============================
// ovrUriScheme_File::ovrUriScheme_File
ovrUriScheme_File::ovrUriScheme_File(char const* schemeName) : ovrUriScheme(schemeName) {}

//==============================
// ovrUriScheme_File::AllocStream_Internal
ovrStream* ovrUriScheme_File::AllocStream_Internal() const {
    return new ovrStream_File(*this);
}

//==============================
// ovrUriScheme_File::HostExists_Internal
bool ovrUriScheme_File::HostExists_Internal(char const* hostName) const {
    return FindHostIndexByHostName(hostName) >= 0;
}

//==============================
// ovrUriScheme_File::OpenHost_Internal
bool ovrUriScheme_File::OpenHost_Internal(char const* hostName, char const* uriSource) {
    int index = FindHostIndexByHostName(hostName);
    if (index >= 0) {
        assert(index < 0); // host already exists
        return false;
    }
    // TODO: Add AllocHost() / AllocHost_Internal() to ovrUriScheme? Requires an ovrUriHost base
    // class, though...
    ovrFileHost* host = new ovrFileHost(hostName, uriSource);
    assert(host != NULL);

    if (!host->Open()) {
        return false;
    }
    Hosts.push_back(host);
    return true;
}

//==============================
// ovrUriScheme_File::CloseHost_Internal
void ovrUriScheme_File::CloseHost_Internal(char const* hostName) {
    ovrFileHost* host = FindHostByHostName(hostName);
    assert(host != NULL);
    if (host != NULL) {
        host->Close();
    }
}

//==============================
// ovrUriScheme_File::
void ovrUriScheme_File::Shutdown_Internal() {}

//==============================
// ovrUriScheme_File::FindHostIndexByHostName
int ovrUriScheme_File::FindHostIndexByHostName(char const* hostName) const {
    if ((hostName == NULL || hostName[0] == '\0') && Hosts.size() > 0) {
        return 0;
    }

    for (int i = 0; i < static_cast<int>(Hosts.size()); ++i) {
        assert(Hosts[i] != NULL);
        if (OVR::OVR_strcmp(Hosts[i]->GetHostName(), hostName) == 0) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrUriScheme_File::FindHostByHostName
ovrUriScheme_File::ovrFileHost* ovrUriScheme_File::FindHostByHostName(char const* hostName) const {
    int index = FindHostIndexByHostName(hostName);
    if (index < 0) {
        return NULL;
    }
    return Hosts[index];
}

//==============================
// ovruriScheme_File::AddHostSourceUri
void ovrUriScheme_File::AddHostSourceUri(char const* hostName, char const* sourceUri) {
    ovrFileHost* host = FindHostByHostName(hostName);
    if (host == NULL) {
        OpenHost(hostName, sourceUri);
        return;
    }
    host->AddSourceUri(sourceUri);
}

//==============================
// ovrUriScheme_File::ovrFileHost::Open
bool ovrUriScheme_File::ovrFileHost::Open() {
    return true; // file schemes don't need to do anything to open a host
}

//==============================
// ovrUriScheme_File::ovrFileHost::Close
void ovrUriScheme_File::ovrFileHost::Close() {
    // file schemes don't need to do anything to open a host
}

//==============================
// ovrUriScheme_File::ovrFileHost::AddSourceUri
void ovrUriScheme_File::ovrFileHost::AddSourceUri(char const* sourceUri) {
    SourceUris.push_back(std::string(sourceUri));
}

//==============================================================================================
// ovrStream_File
//==============================================================================================

//==============================
// ovrStream_File::ovrStream_File
ovrStream_File::ovrStream_File(ovrUriScheme const& scheme) : ovrStream(scheme), F(NULL) {}

//==============================
// ovrStream_File::~ovrStream_File
ovrStream_File::~ovrStream_File() {}

//==============================
// ovrStream_File::GetLocalPathFromUri_Internal
bool ovrStream_File::GetLocalPathFromUri_Internal(const char* uri, std::string& outputPath) {
    // require a fully-qualified Uri for now?
    char schemeName[128];
    char hostName[128];
    int port;
    char uriPath[ovrFileSys::OVR_MAX_SCHEME_LEN];
    if (!ovrUri::ParseUri(
            uri,
            schemeName,
            sizeof(schemeName),
            NULL,
            0,
            NULL,
            0,
            hostName,
            sizeof(hostName),
            port,
            uriPath,
            sizeof(uriPath),
            NULL,
            0,
            NULL,
            0)) {
        ALOG("ovrStream_File::GetLocalPathFromUri_Internal: invalid uri '%s'", uri);
        assert(false);
        return false;
    }
    size_t uriPathLen = UTF8Util::GetLength(uriPath);
    if (uriPathLen < 1) {
        assert(uriPathLen > 0);
        return false;
    }
    // on Windows, the URI path may have a /C:/ pattern, in which case we must skip over the leading
    // slash AND not prepend the host's sourceUri
    char fullPath[ovrFileSys::OVR_MAX_PATH_LEN];
    if (UriPathStartsWithDriveLetter(uriPath)) {
        OVR::OVR_sprintf(fullPath, sizeof(fullPath), "%s", SafePathFromUriPath(uriPath));
        outputPath = fullPath;
        return true;
    }

    ovrUriScheme_File::ovrFileHost* host = GetFileScheme().FindHostByHostName(hostName);
    if (host == NULL) {
        assert(host != NULL);
        return false;
    }

    // return the path from the last host
    int i = host->GetNumSourceUris() - 1;
    if (i >= 0) {
        // find the host's base path
        char const* sourceUri = host->GetSourceUri(i);
        assert(sourceUri != NULL);
        // in this case, the URI path should ALWAYS have a leading slash!
        assert(uriPath[0] == '/');
        // convert the source uri into a system path
        char basePath[ovrFileSys::OVR_MAX_PATH_LEN];
        if (!ovrUri::ParseUri(
                sourceUri,
                NULL,
                0,
                NULL,
                0,
                NULL,
                0,
                NULL,
                0,
                port,
                basePath,
                sizeof(basePath),
                NULL,
                0,
                NULL,
                0)) {
            ALOG(
                "ovrStream_File::GetLocalPathFromUri_Internal: invalid source uri '%s'", sourceUri);
            assert(false);
            return false;
        }
        AppendUriPath(SafePathFromUriPath(basePath), uriPath, fullPath, sizeof(fullPath));
        outputPath = fullPath;
        return true;
    }

    return false;
}

//==============================
// ovrStream_File::Open_Internal
bool ovrStream_File::Open_Internal(char const* uri, ovrStreamMode const mode) {
    if (F != NULL) {
        assert(F == NULL);
        ALOG("Attempted to open file '%s' with an already open file handle.", uri);
        return false;
    }
    char const* fmode = NULL;
    switch (mode) {
        case OVR_STREAM_MODE_READ:
            fmode = "rb";
            break;
        case OVR_STREAM_MODE_WRITE:
            fmode = "wb";
            break;
        default:
            assert(false);
            return false;
    }
    if (fmode == NULL) {
        assert(fmode != NULL);
        return false;
    }

    // require a fully-qualified Uri for now?
    char schemeName[128];
    char hostName[128];
    int port;
    char uriPath[ovrFileSys::OVR_MAX_SCHEME_LEN];
    if (!ovrUri::ParseUri(
            uri,
            schemeName,
            sizeof(schemeName),
            NULL,
            0,
            NULL,
            0,
            hostName,
            sizeof(hostName),
            port,
            uriPath,
            sizeof(uriPath),
            NULL,
            0,
            NULL,
            0)) {
        ALOG("ovrStream_File::Open_Internal: invalid uri '%s'", uri);
        assert(false);
        return false;
    }
    size_t uriPathLen = UTF8Util::GetLength(uriPath);
    if (uriPathLen < 1) {
        assert(uriPathLen > 0);
        return false;
    }
    // on Windows, the URI path may have a /C:/ pattern, in which case we must skip over the leading
    // slash AND not prepend the host's sourceUri
    char fullPath[ovrFileSys::OVR_MAX_PATH_LEN];
    if (UriPathStartsWithDriveLetter(uriPath)) {
        OVR::OVR_sprintf(fullPath, sizeof(fullPath), "%s", SafePathFromUriPath(uriPath));
        F = fopen(fullPath, fmode);
        if (F != NULL) {
            Uri = uri;
            return true;
        }
        return false;
    }

    ovrUriScheme_File::ovrFileHost* host = GetFileScheme().FindHostByHostName(hostName);
    if (host == NULL) {
        assert(host != NULL);
        return false;
    }

    // open the file in the first host path where it exists
    for (int i = host->GetNumSourceUris() - 1; i >= 0; --i) {
        // find the host's base path
        char const* sourceUri = host->GetSourceUri(i);
        assert(sourceUri != NULL);
        // in this case, the URI path should ALWAYS have a leading slash!
        assert(uriPath[0] == '/');
        // convert the source uri into a system path
        char basePath[ovrFileSys::OVR_MAX_PATH_LEN];
        if (!ovrUri::ParseUri(
                sourceUri,
                NULL,
                0,
                NULL,
                0,
                NULL,
                0,
                NULL,
                0,
                port,
                basePath,
                sizeof(basePath),
                NULL,
                0,
                NULL,
                0)) {
            ALOG("ovrStream_File::Open_Internal: invalid source uri '%s'", sourceUri);
            assert(false);
            return false;
        }
        AppendUriPath(SafePathFromUriPath(basePath), uriPath, fullPath, sizeof(fullPath));
        // OVR_sprintf( fullPath, sizeof( fullPath ), "%s%s", ovrPathUtils::SafePathFromUriPath(
        // basePath ), uriPath );
        F = fopen(fullPath, fmode);
        if (F != NULL) {
            Uri = uri;
            return true;
        }
    }
    return false;
}

//==============================
// ovrStream_File::Close_Internal
void ovrStream_File::Close_Internal() {
    if (F != NULL) {
        fclose(F);
        F = NULL;
    }
}

//==============================
// ovrStream_File::Flush_Internal
void ovrStream_File::Flush_Internal() {
    if (F != NULL) {
        fflush(F);
    }
}

//==============================
// ovrStream_File::Read_Internal
bool ovrStream_File::Read_Internal(
    std::vector<uint8_t>& outBuffer,
    size_t const bytesToRead,
    size_t& outBytesRead) {
    size_t numRead;
    numRead = fread(outBuffer.data(), bytesToRead, 1, F);
    outBytesRead = numRead * bytesToRead;
    if (numRead != 1) {
        ALOG(
            "Tried to read %zu bytes from file '%s', but only read %zu bytes.",
            bytesToRead,
            Uri.c_str(),
            outBytesRead);
        return false;
    }
    return true;
}

//==============================
// ovrStream_File::ReadFile_Internal
bool ovrStream_File::ReadFile_Internal(std::vector<uint8_t>& outBuffer) {
    std::vector<uint8_t> buffer(Length());
    outBuffer = buffer;
    size_t bytesRead = 0;
    return Read_Internal(outBuffer, outBuffer.size(), bytesRead);
}

//==============================
// ovrStream_File::Write_Internal
bool ovrStream_File::Write_Internal(void const* inBuffer, size_t const bytesToWrite) {
    size_t recsWritten = fwrite(inBuffer, bytesToWrite, 1, F);
    if (recsWritten != 1) {
        ALOG("Failed to write %zu bytes to file '%s'", bytesToWrite, Uri.c_str());
        return false;
    }
    return true;
}

//==============================
// ovrStream_File::Tell_Internal
size_t ovrStream_File::Tell_Internal() const {
    return ftell(F);
}

//==============================
// ovrStream_File::Length_Internal
size_t ovrStream_File::Length_Internal() const {
    // remember the current offset
    size_t curOfs = ftell(F);
    // seek to the end
    fseek(F, 0, SEEK_END);
    // get the position at the end
    size_t len = ftell(F);
    // seek back to the original offset
    fseek(F, curOfs, SEEK_SET);
    return len;
}

//==============================
// ovrStream_File::AtEnd_Internal
bool ovrStream_File::AtEnd_Internal() const {
    return feof(F) != 0;
}

//==============================================================================================
// ovrUriScheme_Apk
//==============================================================================================

//==============================
// ovrUriScheme_Apk::ovrUriScheme_Apk
ovrUriScheme_Apk::ovrUriScheme_Apk(char const* schemeName) : ovrUriScheme(schemeName) {}

//==============================
// ovrUriScheme_Apk::~ovrUriScheme_Apk
ovrUriScheme_Apk::~ovrUriScheme_Apk() {}

//==============================
// ovrUriScheme_Apk::AllocStream_Internal
ovrStream* ovrUriScheme_Apk::AllocStream_Internal() const {
    return new ovrStream_Apk(*this);
}

//==============================
// ovrUriScheme_Apk::
bool ovrUriScheme_Apk::HostExists_Internal(char const* hostName) const {
    return FindHostIndexByHostName(hostName) >= 0;
}

//==============================
// ovrUriScheme_Apk::OpenHost_Internal
bool ovrUriScheme_Apk::OpenHost_Internal(char const* hostName, char const* sourceUri) {
    int index = FindHostIndexByHostName(hostName);
    if (index >= 0) {
        assert(index < 0); // host already exists
        return false;
    }
    ovrApkHost* host = new ovrApkHost(hostName, sourceUri);
    assert(host != NULL);

    if (!host->Open()) {
        return false;
    }
    Hosts.push_back(host);
    return true;
}

//==============================
// ovrUriScheme_Apk::CloseHost_Internal
void ovrUriScheme_Apk::CloseHost_Internal(char const* hostName) {
    ovrApkHost* host = FindHostByHostName(hostName);
    assert(host != NULL);
    if (host != NULL) {
        host->Close();
    }
}

//==============================
// ovrUriScheme_Apk::FindHostIndexByHostName
int ovrUriScheme_Apk::FindHostIndexByHostName(char const* hostName) const {
    if ((hostName == NULL || hostName[0] == '\0') && Hosts.size() > 0) {
        return 0;
    }

    for (int i = 0; i < static_cast<int>(Hosts.size()); ++i) {
        assert(Hosts[i] != NULL);
        if (OVR::OVR_strcmp(Hosts[i]->GetHostName(), hostName) == 0) {
            return i;
        }
    }
    return -1;
}

//==============================
// ovrUriScheme_Apk::FindHostByHostName
ovrUriScheme_Apk::ovrApkHost* ovrUriScheme_Apk::FindHostByHostName(char const* hostName) const {
    int index = FindHostIndexByHostName(hostName);
    if (index < 0) {
        return NULL;
    }
    return Hosts[index];
}

//==============================
// ovrUriScheme_Apk::GetZipFileForHostName
void* ovrUriScheme_Apk::GetZipFileForHostName(char const* hostName) const {
    ovrApkHost* host = FindHostByHostName(hostName);
    if (host == NULL) {
        assert(host != NULL);
        return NULL;
    }
    return host->GetZipFile();
}

//==============================
// ovrUriScheme_Apk::ovrApkHost::Open
bool ovrUriScheme_Apk::ovrApkHost::Open() {
    char scheme[ovrFileSys::OVR_MAX_SCHEME_LEN];
    char username[128];
    char password[128];
    char hostName[ovrFileSys::OVR_MAX_HOST_NAME_LEN];
    int port;
    char path[ovrFileSys::OVR_MAX_PATH_LEN];
    char query[1024];
    char fragment[1024];

    bool const valid = ovrUri::ParseUri(
        SourceUri.c_str(),
        scheme,
        sizeof(scheme),
        username,
        sizeof(username),
        password,
        sizeof(password),
        hostName,
        sizeof(hostName),
        port,
        path,
        sizeof(path),
        query,
        sizeof(query),
        fragment,
        sizeof(fragment));

    // we expect apks to use a file scheme
    OVR_UNUSED(valid);
    assert(valid && OVR::OVR_stricmp(scheme, "file") == 0);

    ZipFile = ovr_OpenOtherApplicationPackage(path);
    if (ZipFile == NULL) {
        ALOG("Failed to open apk: '%s'", SourceUri.c_str());
        return false;
    }
    return true;
}

//==============================
// ovrUriScheme_Apk::ovrApkHost::Close
void ovrUriScheme_Apk::ovrApkHost::Close() {
    assert(ZipFile != NULL);
    if (ZipFile != NULL) {
        ovr_CloseOtherApplicationPackage(ZipFile);
    }
    assert(ZipFile == NULL);
}

//==============================
// ovrUriScheme_Apk::Shutdown_Internal
void ovrUriScheme_Apk::Shutdown_Internal() {
    // close all hosts
    for (int i = 0; i < static_cast<int>(Hosts.size()); ++i) {
        Hosts[i]->Close();
        delete Hosts[i];
        Hosts[i] = NULL;
    }
    Hosts.clear();
}

//==============================================================================================
// ovrStream_Apk
//==============================================================================================

//==============================
// ovrStream_Apk::ovrStream_Apk
ovrStream_Apk::ovrStream_Apk(ovrUriScheme const& scheme) : ovrStream(scheme), IsOpen(false) {}

//==============================
// ovrStream_Apk::~ovrStream_Apk
ovrStream_Apk::~ovrStream_Apk() {}

//==============================
// ovrStream_Apk::GetLocalPathFromUri_Internal
bool ovrStream_Apk::GetLocalPathFromUri_Internal(const char* uri, std::string& outputPath) {
    // can't get path to file inside of zip
    return false;
}

//==============================
// ovrStream_Apk::Open_Internal
bool ovrStream_Apk::Open_Internal(char const* uri, ovrStreamMode const mode) {
    if (IsOpen) {
        ALOG("ovrStream_Apk: tried to open uri '%s' when '%s' is already open", uri, GetUri());
        assert(!IsOpen);
        return false;
    }

    if (mode != OVR_STREAM_MODE_READ) {
        assert(mode == OVR_STREAM_MODE_READ);
        ALOG("Only OVR_STREAM_MODE_READ is supported for apks! Uri: '%s'", uri);
        return false;
    }

    char hostName[ovrFileSys::OVR_MAX_HOST_NAME_LEN];
    int port;
    char path[ovrFileSys::OVR_MAX_SCHEME_LEN];
    if (!ovrUri::ParseUri(
            uri,
            NULL,
            0,
            NULL,
            0,
            NULL,
            0,
            hostName,
            sizeof(hostName),
            port,
            path,
            sizeof(path),
            NULL,
            0,
            NULL,
            0)) {
        ALOG("ovrStream_Apk::Open_Internal: invalid Uri '%s'", uri);
        return false;
    }

    // get the zip file for this host
    void* zipFile = GetApkScheme().GetZipFileForHostName(hostName);
    if (zipFile == NULL) {
        ALOG("ovrStream_Apk::Open_Internal: no zip file for uri '%s', host '%s'", uri, hostName);
        return false;
    }

    // inside of zip files, the leading slash will cause the file to not be found, so skip it
    char const* pathStart = (path[0] == '/') ? path + 1 : path;
    IsOpen = ovr_OtherPackageFileExists(zipFile, pathStart);
    return IsOpen;
}

//==============================
// ovrStream_Apk::Close_Internal
void ovrStream_Apk::Close_Internal() {
    IsOpen = false;
}

//==============================
// ovrStream_Apk::Flush_Internal
void ovrStream_Apk::Flush_Internal() {
    // no-op
}

//==============================
// ovrStream_Apk::Read_Internal
bool ovrStream_Apk::Read_Internal(
    std::vector<uint8_t>& outBuffer,
    size_t const bytesToRead,
    size_t& outBytesRead) {
    assert(false); // TODO: cannot read partial files from an apk yet
    return false;
}

//==============================
// ovrStream_Apk::ReadFile_Internal
bool ovrStream_Apk::ReadFile_Internal(std::vector<uint8_t>& outBuffer) {
    char hostName[ovrFileSys::OVR_MAX_HOST_NAME_LEN];
    int port;
    char path[ovrFileSys::OVR_MAX_SCHEME_LEN];
    if (!ovrUri::ParseUri(
            GetUri(),
            NULL,
            0,
            NULL,
            0,
            NULL,
            0,
            hostName,
            sizeof(hostName),
            port,
            path,
            sizeof(path),
            NULL,
            0,
            NULL,
            0)) {
        ALOG("ovrStream_Apk::ReadFile_Internal: invalid Uri '%s'", GetUri());
        return false;
    }

    void* zipFile = GetApkScheme().GetZipFileForHostName(hostName);

    // inside of zip files, the leading slash will cause the file to not be found, so skip it
    char const* pathStart = (path[0] == '/') ? path + 1 : path;

    return ovr_ReadFileFromOtherApplicationPackage(zipFile, pathStart, outBuffer);
}

//==============================
// ovrStream_Apk::Write_Internal
bool ovrStream_Apk::Write_Internal(void const* inBuffer, size_t const bytesToWrite) {
    assert(false); // can never write to an apk
    return false;
}

//==============================
// ovrStream_Apk::Tell_Internal
size_t ovrStream_Apk::Tell_Internal() const {
    assert(false); // TODO: cannot read partial files from an apk yet
    return 0;
}

//==============================
// ovrStream_Apk::Length_Internal
size_t ovrStream_Apk::Length_Internal() const {
    assert(false); // TODO: cannot read partial files from an apk yet
    return 0;
}

//==============================
// ovrStream_Apk::AtEnd_Internal
bool ovrStream_Apk::AtEnd_Internal() const {
    assert(false); // TODO: cannot read partial files from an apk yet
    return true;
}

} // namespace OVRFW
