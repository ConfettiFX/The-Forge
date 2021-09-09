/************************************************************************************

Filename    :   OVR_TextureManager.cpp
Content     :   Keeps track of textures so they don't need to be loaded more than once.
Created     :   1/22/2016
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

// Make sure we get PRIu64
#define __STDC_FORMAT_MACROS 1

#include "TextureManager.h"

#include "Misc/Log.h"

#include <vector>
#include <unordered_map>

#include "OVR_LogUtils.h"
#include "OVR_FileSys.h"
#include "PackageFiles.h"

namespace OVRFW {

//==============================================================================================
// ovrManagedTexture
//==============================================================================================

//==============================
// ovrManagedTexture::Free
void ovrManagedTexture::Free() {
    FreeTexture(Texture);
    Source = TEXTURE_SOURCE_MAX;
    Uri = "";
    IconId = -1;
    Handle = textureHandle_t();
}

//==============================================================================================
// ovrTextureManagerImpl
//==============================================================================================

//==============================================================
// ovrTextureManagerImpl
class ovrTextureManagerImpl : public ovrTextureManager {
   public:
    friend class ovrTextureManager;

    virtual void Init() OVR_OVERRIDE;
    virtual void Shutdown() OVR_OVERRIDE;

    virtual textureHandle_t LoadTexture(
        ovrFileSys& fileSys,
        char const* uri,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) OVR_OVERRIDE;
    virtual textureHandle_t LoadTexture(
        char const* uri,
        void const* buffer,
        size_t const bufferSize,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) OVR_OVERRIDE;
    virtual textureHandle_t LoadRGBATexture(
        char const* uri,
        void const* imageData,
        int const imageWidth,
        int const imageHeight,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) OVR_OVERRIDE;
    virtual textureHandle_t LoadRGBATexture(
        int const iconId,
        void const* imageData,
        int const imageWidth,
        int const imageHeight,
        ovrTextureFilter const filterType = FILTER_DEFAULT,
        ovrTextureWrap const wrapType = WRAP_DEFAULT) OVR_OVERRIDE;

    virtual void FreeTexture(textureHandle_t const handle) OVR_OVERRIDE;

    virtual ovrManagedTexture GetTexture(textureHandle_t const handle) const OVR_OVERRIDE;
    virtual GlTexture GetGlTexture(textureHandle_t const handle) const OVR_OVERRIDE;

    virtual textureHandle_t GetTextureHandle(char const* uri) const OVR_OVERRIDE;
    virtual textureHandle_t GetTextureHandle(int const iconId) const OVR_OVERRIDE;

    virtual void PrintStats() const OVR_OVERRIDE;

   private:
    std::vector<ovrManagedTexture> Textures;
    std::vector<int> FreeTextures;
    bool Initialized;
    std::unordered_map<std::string, int> UriHash;

    mutable int NumUriLoads;
    mutable int NumActualUriLoads;
    mutable int NumBufferLoads;
    mutable int NumActualBufferLoads;
    mutable int NumStringSearches;
    mutable int NumStringCompares;
    mutable int NumSearches;
    mutable int NumCompares;

   private:
    ovrTextureManagerImpl();
    virtual ~ovrTextureManagerImpl();

    int FindTextureIndex(char const* uri) const;
    int FindTextureIndex(int const iconId) const;
    int IndexForHandle(textureHandle_t const handle) const;
    textureHandle_t AllocTexture();

    static void SetTextureWrapping(GlTexture& tex, ovrTextureWrap const wrapType);
    static void SetTextureFiltering(GlTexture& tex, ovrTextureFilter const filterType);
};

//==============================
// ovrTextureManagerImpl::
ovrTextureManagerImpl::ovrTextureManagerImpl()
    : Initialized(false),
      NumUriLoads(0),
      NumActualUriLoads(0),
      NumBufferLoads(0),
      NumActualBufferLoads(0),
      NumStringSearches(0),
      NumStringCompares(0),
      NumSearches(0),
      NumCompares(0) {}

//==============================
// ovrTextureManagerImpl::
ovrTextureManagerImpl::~ovrTextureManagerImpl() {
    assert(!Initialized); // call Shutdown() explicitly
}

//==============================
// ovrTextureManagerImpl::
void ovrTextureManagerImpl::Init() {
    UriHash.reserve(512);
    Initialized = true;
}

//==============================
// ovrTextureManagerImpl::
void ovrTextureManagerImpl::Shutdown() {
    for (auto& texture : Textures) {
        if (texture.IsValid()) {
            texture.Free();
        }
    }

    Textures.resize(0);
    FreeTextures.resize(0);
    UriHash.clear();

    Initialized = false;
}

//==============================
// ovrTextureManagerImpl::SetTextureWrapping
void ovrTextureManagerImpl::SetTextureWrapping(GlTexture& tex, ovrTextureWrap const wrapType) {
    /// OVR_PERF_TIMER( SetTextureWrapping );
    switch (wrapType) {
        case WRAP_DEFAULT:
            return;
        case WRAP_CLAMP:
            MakeTextureClamped(tex);
            break;
        case WRAP_REPEAT:
            glBindTexture(tex.target, tex.texture);
            glTexParameteri(tex.target, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(tex.target, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glBindTexture(tex.target, 0);
            break;
        default:
            assert(false);
            break;
    }
}

//==============================
// ovrTextureManagerImpl::SetTextureFiltering
void ovrTextureManagerImpl::SetTextureFiltering(GlTexture& tex, ovrTextureFilter const filterType) {
    /// OVR_PERF_TIMER( SetTextureFiltering );
    switch (filterType) {
        case FILTER_DEFAULT:
            return;
        case FILTER_LINEAR:
            MakeTextureLinear(tex);
            break;
        case FILTER_MIPMAP_LINEAR:
            MakeTextureLinearNearest(tex);
            break;
        case FILTER_MIPMAP_TRILINEAR:
            MakeTextureTrilinear(tex);
            break;
        case FILTER_MIPMAP_ANISOTROPIC2:
            MakeTextureAniso(tex, 2.0f);
            break;
        case FILTER_MIPMAP_ANISOTROPIC4:
            MakeTextureAniso(tex, 4.0f);
            break;
        case FILTER_MIPMAP_ANISOTROPIC8:
            MakeTextureAniso(tex, 8.0f);
            break;
        case FILTER_MIPMAP_ANISOTROPIC16:
            MakeTextureAniso(tex, 16.0f);
            break;
        default:
            assert(false);
    }
}

//==============================
// ovrTextureManagerImpl::LoadTexture
textureHandle_t ovrTextureManagerImpl::LoadTexture(
    ovrFileSys& fileSys,
    char const* uri,
    ovrTextureFilter const filterType,
    ovrTextureWrap const wrapType) {
    /// OVR_PERF_TIMER( LoadTexture_FromFile );

    NumUriLoads++;

    int idx = FindTextureIndex(uri);
    if (idx >= 0) {
        return Textures[idx].GetHandle();
    }

    int w;
    int h;
    GlTexture tex = LoadTextureFromUri(fileSys, uri, TextureFlags_t(TEXTUREFLAG_NO_DEFAULT), w, h);
    if (!tex.IsValid()) {
        ALOG("LoadTextureFromUri( '%s' ) failed!", uri);
        return textureHandle_t();
    }

    textureHandle_t handle = AllocTexture();
    if (handle.IsValid()) {
        SetTextureWrapping(tex, wrapType);
        SetTextureFiltering(tex, filterType);

        idx = IndexForHandle(handle);
        Textures[idx] = ovrManagedTexture(handle, uri, tex);
        UriHash[std::string(uri)] = idx;

        NumActualUriLoads++;
    }

    return handle;
}

//==============================
// ovrTextureManagerImpl::LoadTexture
textureHandle_t ovrTextureManagerImpl::LoadTexture(
    char const* uri,
    void const* buffer,
    size_t const bufferSize,
    ovrTextureFilter const filterType,
    ovrTextureWrap const wrapType) {
    /// OVR_PERF_TIMER( LoadTexture_FromBuffer );

    NumBufferLoads++;

    int idx = FindTextureIndex(uri);
    if (idx >= 0) {
        return Textures[idx].GetHandle();
    }

    int width = 0;
    int height = 0;
    // NOTE: buffer ownership handled by caller
    GlTexture tex = LoadTextureFromBuffer(
        uri,
        static_cast<uint8_t const*>(buffer),
        bufferSize,
        TextureFlags_t(TEXTUREFLAG_NO_DEFAULT),
        width,
        height);

    if (!tex.IsValid()) {
        ALOG(
            "LoadTextureFromBuffer( '%s', %p, %u ) failed!",
            uri,
            buffer,
            static_cast<uint32_t>(bufferSize));
        return textureHandle_t();
    }

    textureHandle_t handle = AllocTexture();
    if (handle.IsValid()) {
        /// OVR_PERF_TIMER( LoadTexture_FromBuffer_IsValid );
        SetTextureWrapping(tex, wrapType);
        SetTextureFiltering(tex, filterType);

        idx = IndexForHandle(handle);
        Textures[idx] = ovrManagedTexture(handle, uri, tex);
        {
            /// OVR_PERF_TIMER( LoadTexture_FromBuffer_Hash );
            UriHash[std::string(uri)] = idx;
        }

        NumActualBufferLoads++;
    }

    return handle;
}

//==============================
// ovrTextureManagerImpl::LoadRGBATexture
textureHandle_t ovrTextureManagerImpl::LoadRGBATexture(
    char const* uri,
    void const* imageData,
    int const imageWidth,
    int const imageHeight,
    ovrTextureFilter const filterType,
    ovrTextureWrap const wrapType) {
    /// OVR_PERF_TIMER( LoadRGBATexture_uri );

    ALOG("LoadRGBATexture: uri = '%s' ", uri);

    NumBufferLoads++;
    if (imageData == nullptr || imageWidth <= 0 || imageHeight <= 0) {
        return textureHandle_t();
    }

    int idx = FindTextureIndex(uri);
    if (idx >= 0) {
        return Textures[idx].GetHandle();
    }

    GlTexture tex;
    {
        /// OVR_PERF_TIMER( LoadRGBATexture_uri_LoadRGBATextureFromMemory );
        tex = LoadRGBATextureFromMemory(
            static_cast<const unsigned char*>(imageData), imageWidth, imageHeight, false);
        if (!tex.IsValid()) {
            // ALOG( "LoadRGBATextureFromMemory( '%s', %p, %i, %i ) failed!", uri, imageData,
            // imageWidth, imageHeight );
            return textureHandle_t();
        }
    }

    textureHandle_t handle = AllocTexture();
    if (handle.IsValid()) {
        /// OVR_PERF_TIMER( LoadRGBATexture_uri_IsValid );
        SetTextureWrapping(tex, wrapType);
        SetTextureFiltering(tex, filterType);

        idx = IndexForHandle(handle);
        Textures[idx] = ovrManagedTexture(handle, uri, tex);
        {
            /// OVR_PERF_TIMER( LoadRGBATexture_uri_Hash );
            UriHash[std::string(uri)] = idx;
        }
        NumActualBufferLoads++;
    }
    return handle;
}

//==============================
// ovrTextureManagerImpl::LoadRGBATexture
textureHandle_t ovrTextureManagerImpl::LoadRGBATexture(
    int const iconId,
    void const* imageData,
    int const imageWidth,
    int const imageHeight,
    ovrTextureFilter const filterType,
    ovrTextureWrap const wrapType) {
    /// OVR_PERF_TIMER( LoadRGBATexture_icon );

    NumBufferLoads++;
    if (imageData == nullptr || imageWidth <= 0 || imageHeight <= 0) {
        return textureHandle_t();
    }

    int idx = FindTextureIndex(iconId);
    if (idx >= 0) {
        return Textures[idx].GetHandle();
    }

    GlTexture tex;
    {
        /// OVR_PERF_TIMER( LoadRGBATexture_icon_LoadRGBATextureFromMemory );
        tex = LoadRGBATextureFromMemory(
            static_cast<const unsigned char*>(imageData), imageWidth, imageHeight, false);
        if (!tex.IsValid()) {
            // ALOG( "LoadRGBATextureFromMemory( %d, %p, %i, %i ) failed!", iconId, imageData,
            // imageWidth, imageHeight );
            return textureHandle_t();
        }
    }

    textureHandle_t handle = AllocTexture();
    if (handle.IsValid()) {
        SetTextureWrapping(tex, wrapType);
        SetTextureFiltering(tex, filterType);

        idx = IndexForHandle(handle);
        Textures[idx] = ovrManagedTexture(handle, iconId, tex);

        NumActualBufferLoads++;
    }
    return handle;
}

//==============================
// ovrTextureManagerImpl::GetTexture
ovrManagedTexture ovrTextureManagerImpl::GetTexture(textureHandle_t const handle) const {
    int idx = IndexForHandle(handle);
    if (idx < 0) {
        return ovrManagedTexture();
    }
    return Textures[idx];
}

//==============================
// ovrTextureManagerImpl::GetGlTexture
GlTexture ovrTextureManagerImpl::GetGlTexture(textureHandle_t const handle) const {
    int idx = IndexForHandle(handle);
    if (idx < 0) {
        return GlTexture();
    }
    return Textures[idx].GetTexture();
}

//==============================
// ovrTextureManagerImpl::FreeTexture
void ovrTextureManagerImpl::FreeTexture(textureHandle_t const handle) {
    int idx = IndexForHandle(handle);
    if (idx >= 0) {
        if (Textures[idx].GetUri().empty()) {
            UriHash.erase(Textures[idx].GetUri());
        }
        Textures[idx].Free();
        FreeTextures.push_back(idx);
    }
}

//==============================
// ovrTextureManagerImpl::FindTextureIndex
int ovrTextureManagerImpl::FindTextureIndex(char const* uri) const {
    /// OVR_PERF_TIMER( FindTextureIndex_uri );

    NumStringSearches++;

    // enable this to always return the first texture. This basically allows
    // texture file loads to be eliminated during perf testing for purposes
    // of comparison
#if 0
	if ( Textures.size() > 0 )
	{
		return 0;
	}
#endif

    auto it = UriHash.find(std::string(uri));
    if (it != UriHash.end()) {
        return it->second;
    }

    return -1;
}

//==============================
// ovrTextureManagerImpl::FindTextureIndex
int ovrTextureManagerImpl::FindTextureIndex(int const iconId) const {
    /// OVR_PERF_TIMER( FindTextureIndex_iconId );

    NumSearches++;
    if (Textures.size() > 0) {
        return 0;
    }
    for (int i = 0; i < static_cast<int>(Textures.size()); ++i) {
        if (Textures[i].IsValid() && Textures[i].GetIconId() == iconId) {
            NumCompares += i;
            return i;
        }
    }
    NumCompares += static_cast<int>(Textures.size());
    return -1;
}

//==============================
// ovrTextureManagerImpl::IndexForHandle
int ovrTextureManagerImpl::IndexForHandle(textureHandle_t const handle) const {
    if (!handle.IsValid()) {
        return -1;
    }
    return handle.Get();
}

//==============================
// ovrTextureManagerImpl::AllocTexture
textureHandle_t ovrTextureManagerImpl::AllocTexture() {
    /// OVR_PERF_TIMER( AllocTexture );

    if (FreeTextures.size() > 0) {
        int idx = FreeTextures[static_cast<int>(FreeTextures.size()) - 1];
        FreeTextures.pop_back();
        Textures[idx] = ovrManagedTexture();
        return textureHandle_t(idx);
    }

    int idx = static_cast<int>(Textures.size());
    Textures.push_back(ovrManagedTexture());

    return textureHandle_t(idx);
}

//==============================
// ovrTextureManagerImpl::GetTextureHandle
textureHandle_t ovrTextureManagerImpl::GetTextureHandle(char const* uri) const {
    int idx = FindTextureIndex(uri);
    if (idx < 0) {
        return textureHandle_t();
    }
    return Textures[idx].GetHandle();
}

//==============================
// ovrTextureManagerImpl::GetTextureHandle
textureHandle_t ovrTextureManagerImpl::GetTextureHandle(int const iconId) const {
    int idx = FindTextureIndex(iconId);
    if (idx < 0) {
        return textureHandle_t();
    }
    return Textures[idx].GetHandle();
}

//==============================
// ovrTextureManagerImpl::PrintStats
void ovrTextureManagerImpl::PrintStats() const {
    ALOG("NumUriLoads:          %i", NumUriLoads);
    ALOG("NumBufferLoads:       %i", NumBufferLoads);
    ALOG("NumActualUriLoads:    %i", NumActualUriLoads);
    ALOG("NumActualBufferLoads: %i", NumActualBufferLoads);

    ALOG("NumStringSearches: %i", NumStringSearches);
    ALOG("NumStringCompares: %i", NumStringCompares);

    ALOG("NumSearches: %i", NumSearches);
    ALOG("NumCompares: %i", NumCompares);
}

//==============================================================================================
// ovrTextureManager
//==============================================================================================

//==============================
// ovrTextureManager::Create
ovrTextureManager* ovrTextureManager::Create() {
    ovrTextureManagerImpl* m = new ovrTextureManagerImpl();
    m->Init();
    return m;
}

//==============================
// ovrTextureManager::Destroy
void ovrTextureManager::Destroy(ovrTextureManager*& m) {
    if (m != nullptr) {
        m->Shutdown();
        delete m;
        m = nullptr;
    }
}

} // namespace OVRFW
